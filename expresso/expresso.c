#define _POSIX_C_SOURCE 200112L
#include "expresso.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/wait.h>

#define max_tasks 100000

static size_t task_counter; //incrementé dans expresso_task, reinitialisé dans expresso_wait
static size_t task_counter_temporary;
static int activity;
static size_t id; //savoir quelle tache executer
static int total_counter;
static pthread_mutex_t my_mutex;
static pthread_cond_t init;
static pthread_cond_t re_init;
pthread_barrier_t barrier;
static schedule_t my_policy;
static size_t nb_workers;
static int sum_weights;

struct task_info_s {
  void (*work)(void *);
  void * data;
  unsigned int weight;
};

typedef struct s_arg {
  struct task_info_s *my_tasks;
  int terminate;
}s_arg;

typedef struct thread_info {
  size_t thread_id;
  int thread_task_counter;
  schedule_t thread_policy;
  int thread_weight;
}thread_info;

static struct s_arg * task_info = NULL;
pthread_t * child = NULL;
static struct thread_info * per_thread_counter = NULL;


void * set_child_task(void * arg) {
  struct thread_info * my_thread = (struct thread_info *) arg;

  while(1) {
    //creation / assurer la reinitialisation
    pthread_barrier_wait(&barrier);

    pthread_mutex_lock(&my_mutex);
    while(activity == 0 && !task_info->terminate) { //initialisation
      pthread_cond_wait(&init, &my_mutex);
    }
    pthread_mutex_unlock(&my_mutex);


    if (task_info->terminate) {
      break;
    }
    my_thread->thread_policy = my_policy;

    switch(my_thread->thread_policy) { //execution des taches

      case SCHEDULE_DYNAMIC :
        while (id < task_counter_temporary) {
          pthread_mutex_lock(&my_mutex);
          size_t my_id_dynamic = id;
          id++; //operations sur id sont dans le verrou
          total_counter++;
          pthread_mutex_unlock(&my_mutex);

          task_info->my_tasks[my_id_dynamic].work(task_info->my_tasks[my_id_dynamic].data); //le calcul

          my_thread->thread_task_counter++; //a la position i correcte
        }
      break;
      
      case SCHEDULE_STATIC :
        for (size_t i=my_thread->thread_id; i<task_counter_temporary; i+=nb_workers) { //chaque thread n'execute que les taches qui lui corresponde
          task_info->my_tasks[i].work(task_info->my_tasks[i].data); //le calcul
          my_thread->thread_task_counter++;
        }
        pthread_mutex_lock(&my_mutex);
        if ((task_counter_temporary - my_thread->thread_id)%nb_workers != 0) { //pour reference, la meme logique de ce calcul est utilisee dans dot.c pour determiner "int tasks". J'evite que le mutex soit dans la boucle for
          total_counter+= ( ((task_counter_temporary - my_thread->thread_id) - ((task_counter_temporary - my_thread->thread_id)%nb_workers)) / nb_workers ) + 1;
        } else {
          total_counter+= ( ((task_counter_temporary - my_thread->thread_id) - ((task_counter_temporary - my_thread->thread_id)%nb_workers)) / nb_workers ) ;
        }
        pthread_mutex_unlock(&my_mutex);
      break;

      case SCHEDULE_BALANCED :
        while (id < task_counter_temporary) {
          if (my_thread->thread_weight <= sum_weights/nb_workers) {
            pthread_mutex_lock(&my_mutex);
            size_t my_id_balanced = id;
            id++; //operations sur id sont dans le verrou
            total_counter++;
            pthread_mutex_unlock(&my_mutex);

            task_info->my_tasks[my_id_balanced].work(task_info->my_tasks[my_id_balanced].data); //le calcul

            my_thread->thread_task_counter++; //a la position i correcte
            my_thread->thread_weight += task_info->my_tasks[my_id_balanced].weight;
          }
          else { break; }
        }
      break;
    }

    //fin des taches
    pthread_barrier_wait(&barrier);

    pthread_mutex_lock(&my_mutex);
    while (activity == 1) { //on attend la reinitialisation
      pthread_cond_wait(&re_init, &my_mutex);
    }
    pthread_mutex_unlock(&my_mutex);
  }
  return NULL;
}


int expresso_initialize() {
  nb_workers = expresso_worker_count();

  //obtention de la variable d'environnement de la politique d'ordonnancement
  char *expresso_schedule = getenv("EXPRESSO_SCHEDULE");
  if (expresso_schedule) {
    if (strcmp("dynamic", expresso_schedule) == 0) {
      my_policy = SCHEDULE_DYNAMIC;
    }
    else if (strcmp("static", expresso_schedule) == 0) {
      my_policy = SCHEDULE_STATIC;
    }
    else if (strcmp("balanced", expresso_schedule) == 0) {
      my_policy = SCHEDULE_BALANCED;
    }
  }
  else {
    my_policy = SCHEDULE_DYNAMIC; //on utilise dynamic comme politique par defaut
  }

  //allocation des taches (avec un maximum de max_tasks taches en file d'attente)
  task_info = malloc(sizeof(s_arg));
  if (!task_info) {
    perror("fail allocating the task_info\n");
    return 0;
  }
  task_info->my_tasks = (struct task_info_s *)malloc(max_tasks*sizeof(struct task_info_s));
  if (!task_info->my_tasks) {
    perror("fail allocating the task_info->my_tasks\n");
    return 0;
  }
  //initialisation de task_info
  for (size_t i=0; i<max_tasks; ++i) {
    task_info->my_tasks[i].work = NULL;
    task_info->my_tasks[i].data = NULL;
    task_info->my_tasks[i].weight = 0;
  }
  sum_weights = 0;
  activity = 0; //travailleurs inactifs
  task_counter = 0;
  task_counter_temporary = 0;
  task_info->terminate = 0;
  id = max_tasks;
  pthread_mutex_init(&my_mutex, NULL);
  pthread_cond_init(&init, NULL);
  pthread_cond_init(&re_init, NULL);
  pthread_barrier_init(&barrier, NULL, nb_workers+1);

  //allocation des données pour expresso_stats
  per_thread_counter = malloc(nb_workers*sizeof(thread_info));
  if (!per_thread_counter) {
    perror("fail allocating per_thread_counter\n");
    return 0;
  }
  for (size_t i=0; i<nb_workers; ++i) {
    per_thread_counter[i].thread_id = i;
    per_thread_counter[i].thread_task_counter = 0;
    per_thread_counter[i].thread_policy = my_policy;
    per_thread_counter[i].thread_weight = 0;
  }
  total_counter = 0;

  //creation des fils d'execution
  child = (pthread_t *)malloc(nb_workers * sizeof(pthread_t));
  if (!child) {
    perror("fail allocating the threads\n");
    return 0;
  }
  for (size_t i=0; i<nb_workers; ++i) {
    pthread_create(&child[i], NULL, set_child_task, &per_thread_counter[i]); //le fil d'execution reste en veille pour l'instant
  }

  //On s'assure que les threads sont initialisés
  pthread_barrier_wait(&barrier);

  return 0;
}

size_t expresso_worker_count() {
  char *worker_count_char = getenv("EXPRESSO_WORKER_COUNT");
  size_t worker_count = worker_count_char ? atoi(worker_count_char) : 1; //set to 1 if not found, le fil d'execution principale
  return worker_count;
}

schedule_t expresso_schedule_get() {
  return my_policy;
}

void expresso_schedule_set(schedule_t policy) {
  if (policy == SCHEDULE_DYNAMIC || policy == SCHEDULE_STATIC || policy == SCHEDULE_BALANCED) {
    pthread_mutex_lock(&my_mutex);
    my_policy = policy;
    pthread_mutex_unlock(&my_mutex);
  }
  else {
    printf("Schedule '%d' not recognised, not setting new schedule\n", policy);
  }
}

int expresso_task(void (* work) (void *), void * data) {
  if (task_counter > max_tasks) {
    printf("cannot assign more tasks, waiting line full!");
    return task_counter;
  }
  task_info->my_tasks[task_counter].work = work;
  task_info->my_tasks[task_counter].data = data;
  task_counter++;
  return task_counter;
}

int expresso_weighted_task(
  void (* work) (void *), void * data, unsigned int weight
) {
  if (weight < 0) {
    printf("Weight = %d < 0; task not scheduled\n", weight);
    return task_counter;
  }
  if (task_counter >= max_tasks) {
    printf("cannot assign more tasks, waiting line full!");
    return task_counter;
  }
  task_info->my_tasks[task_counter].work = work;
  task_info->my_tasks[task_counter].data = data;
  task_info->my_tasks[task_counter].weight = weight; // > 0
  sum_weights+=weight;
  task_counter++;
  return task_counter;
}

int expresso_wait() {
  pthread_mutex_lock(&my_mutex);
  id = 0;
  activity = 1; //travailleurs actifs
  task_counter_temporary = task_counter;
  pthread_cond_broadcast(&init);
  pthread_mutex_unlock(&my_mutex);

  pthread_barrier_wait(&barrier);

  //reinitialisation
  pthread_mutex_lock(&my_mutex);
  id=max_tasks;
  activity = 0;
  for (size_t i=0; i<task_counter; ++i) {
    task_info->my_tasks[i].work = NULL;
    task_info->my_tasks[i].data = NULL;
    task_info->my_tasks[i].weight = 0;
  }
  for (size_t i=0; i<nb_workers; ++i) {
    per_thread_counter[i].thread_weight = 0;
  }
  task_counter=0;
  sum_weights=0;
  pthread_cond_broadcast(&re_init);
  pthread_mutex_unlock(&my_mutex);

  //On s'assure que les threads sont prets pour les initialisés a nouveau
  pthread_barrier_wait(&barrier);
  return 0;
}

void expresso_stats() {
  double mean_tasks = 0;
  printf("Total tasks executed : %d\n", total_counter);
  for (size_t i=0; i<nb_workers; ++i) {
    printf("Tasks executed by child %zu : %d\n", per_thread_counter[i].thread_id, per_thread_counter[i].thread_task_counter);
    mean_tasks += per_thread_counter[i].thread_task_counter;
  }
  mean_tasks /= nb_workers;
  printf("Mean of tasks executed by children : %f\n", mean_tasks);
 }

double expresso_wall_time() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int expresso_finalize() {
  pthread_mutex_lock(&my_mutex);
  id=0;
  task_info->terminate = 1;
  pthread_cond_broadcast(&init); //on envoi le signal et rentre dans terminate
  pthread_mutex_unlock(&my_mutex);
  for (size_t i=0; i<nb_workers; ++i) { //join chaque fil d'execution
    pthread_join(child[i], NULL);
  }
  pthread_mutex_destroy(&my_mutex);
  pthread_cond_destroy(&init);
  pthread_cond_destroy(&re_init);
  pthread_barrier_destroy(&barrier);
  free(child);
  free(per_thread_counter);
  free(task_info->my_tasks);
  free(task_info);
  return 0;
}
