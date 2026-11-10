#ifndef __EXPRESSO_H
#define __EXPRESSO_H

#include <stdlib.h>

typedef enum {
  SCHEDULE_DYNAMIC,
  SCHEDULE_STATIC,
  SCHEDULE_BALANCED
} schedule_t;

int expresso_initialize();

size_t expresso_worker_count();

schedule_t expresso_schedule_get();

void expresso_schedule_set(schedule_t);

int expresso_task(void (*) (void *), void *);

int expresso_weighted_task(void (*) (void *), void *, unsigned int);

int expresso_wait();

void expresso_stats();

double expresso_wall_time();

int expresso_finalize();

#endif
