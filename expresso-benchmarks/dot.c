/**
 * @file dot.c
 * @brief Computes the dot product of two vectors.
 *
 * This is a benchmark program for the ExPResSO library. It has a reference
 * sequential and a room for a parallel implementation using ExPResSO.
 *
 * This program accepts exactly two optional arguments:
 *   1. size of the vectors,
 *   2. number of elements of the vectors processed by each parallelizable task.
 *
 * The second argument is applicable only to the parallel implementation.
 */

#include <stdio.h>
#include <stdlib.h>
#include "expresso.h"

struct s_data {
  double * a;
  double * b;
  size_t start, end;
  double sum;
};

void dot(void * data) {
  struct s_data * this = (struct s_data *) data;
  for (size_t i=this->start; i<this->end; i++) {
    this->sum += this->a[i] * this->b[i];
  }
}

/**
 * @fn int main(int argc, char ** argv)
 * @arg argc Number of command-line arguments (including the executable name).
 * @arg argv Values of command-line arguments (including the executable name).
 * @brief Main function.
 */
int main(int argc, char ** argv) {
  int size = 1024, block = 64;
  if(argc == 2) {
    fprintf(stderr, "Missing block size!\n");
    return 1;
  }
  if(argc > 2) {
    size = atoi(argv[1]);
    block = atoi(argv[2]);
  }
  if(block > size) {
    block = size;
  }
  
  double * v1 = (double *) malloc((size_t) size * sizeof(double)),
         * v2 = (double *) malloc((size_t) size * sizeof(double));
  double sum_reference = 0.0, sum_expresso = 0.0;
  double time_reference = 0.0, time_expresso = 0.0;

  // Initialization with random values.
  for(int i = 0; i < size; i++) {
    v1[i] = rand() % 10;
    v2[i] = rand() % 10;
  }

  // Reference sequential implementation.
  time_reference = expresso_wall_time();
  for(int i = 0; i < size; i++) {
    sum_reference += v1[i] * v2[i];
  }
  time_reference = expresso_wall_time() - time_reference;

  // TODO: Parallel implementation using ExPResSO.
  int tasks = ((size - size%block)/block) ;
  if (size%block != 0) {
    tasks+=1;
  }
  struct s_data * data = malloc(tasks * sizeof(struct s_data));
  for(int i = 0; i < tasks; i++) {
    data[i].a = v1;
    data[i].b = v2;
    data[i].start = i*block;
    data[i].end = (i+1)*block;
    if (data[i].start + block > size) {
      data[i].end = size;
    } else {
      data[i].end = (i+1)*block;
    }
    data[i].sum = 0.;
  }
  expresso_initialize();
  
  schedule_t my_schedule = expresso_schedule_get();
  if (my_schedule == SCHEDULE_BALANCED) {
    for(int i = 0; i < tasks; i++) {
      expresso_weighted_task(&dot, (void *) &data[i], (unsigned int) data[i].end - data[i].start); //la (reelle) taille du block
    }
  } 
  else { //dynamic / static
    for(int i = 0; i < tasks; i++) {
      expresso_task(&dot, (void *) &data[i]);
    }
  }
  time_expresso = expresso_wall_time();
  expresso_wait();
  for (int i=0 ; i<tasks; i++) {
    sum_expresso += data[i].sum;
  }
  time_expresso = expresso_wall_time() - time_expresso;
  expresso_stats();
  expresso_finalize();
  //end TODO

  printf("Reference result: %g\n", sum_reference);
  printf(" ExPResSO result: %g\n", sum_expresso);

  // Results check.
  if(sum_reference != sum_expresso) {
    fprintf(stderr, "Bad results :-(\n");
    free(v1); free(v2);
    return 1;
  }

  printf("Reference computation time: %g s\n", time_reference);
  printf(" ExPResSO computation time: %g s\n", time_expresso);

  printf("Good results :-)\n");
  free(v1); free(v2); free(data);
  return 0;
}
