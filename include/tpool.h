#ifndef __TPOOL_H__
#define __TPOOL_H__

#include <stdbool.h>
#include <stddef.h>

#include "wrappers.h"

typedef void *(*thread_func_t)(void *arg);

typedef struct tpool_job {
    thread_func_t      func;
    void              *arg;
    struct tpool_job *next;
} tpool_job_t;

typedef struct tpool {
    tpool_job_t    *first; /* pop index */
    tpool_job_t    *last; /* push index */
    pthread_mutex_t  mutex; 
    pthread_cond_t   work_to_be_processed;
    pthread_cond_t   work_in_progress;
    size_t           working_cnt; /* num threads actively working */
    size_t           thread_cnt; /* num alive threads */
    bool             stop; /* used to stop pool */
} tpool_t;

tpool_t *tpool_init(size_t num_threads);
void tpool_destroy(tpool_t *pool);
bool tpool_push(tpool_t *pool, thread_func_t func, void *arg);
void tpool_wait(tpool_t *pool);

#endif 
