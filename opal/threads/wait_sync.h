/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2014-2016 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2016      Los Alamos National Security, LLC. All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "opal/sys/atomic.h"
#include "opal/threads/condition.h"
#include <pthread.h>

BEGIN_C_DECLS

typedef struct ompi_wait_sync_t {
    int32_t count;
    int32_t status;
    pthread_cond_t condition;
    pthread_mutex_t lock;
    struct ompi_wait_sync_t *next;
    struct ompi_wait_sync_t *prev;
} ompi_wait_sync_t;

#define REQUEST_PENDING        (void*)0L
#define REQUEST_COMPLETED      (void*)1L

#define SYNC_WAIT(sync)                 (opal_using_threads() ? sync_wait_mt (sync) : sync_wait_st (sync))

#define WAIT_SYNC_RELEASE(sync)                       \
    if (opal_using_threads()) {                       \
       pthread_cond_destroy(&(sync)->condition);      \
       pthread_mutex_destroy(&(sync)->lock);          \
    }

#define WAIT_SYNC_SIGNAL(sync)                        \
    if (opal_using_threads()) {                       \
        pthread_mutex_lock(&(sync->lock));            \
        pthread_cond_signal(&sync->condition);        \
        pthread_mutex_unlock(&(sync->lock));          \
    }

OPAL_DECLSPEC int sync_wait_mt(ompi_wait_sync_t *sync);
static inline int sync_wait_st (ompi_wait_sync_t *sync)
{
    while (sync->count > 0) {
        opal_progress();
    }

    return sync->status;
}


#define WAIT_SYNC_INIT(sync,c)                                  \
    do {                                                        \
        (sync)->count = c;                                      \
        (sync)->next = NULL;                                    \
        (sync)->prev = NULL;                                    \
        (sync)->status = 0;                                     \
        if (opal_using_threads()) {                             \
            pthread_cond_init (&(sync)->condition, NULL);       \
            pthread_mutex_init (&(sync)->lock, NULL);           \
        }                                                       \
    } while(0)

/**
 * Update the status of the synchronization primitive. If an error is
 * reported the synchronization is completed and the signal
 * triggered. The status of the synchronization will be reported to
 * the waiting threads.
 */
static inline void wait_sync_update(ompi_wait_sync_t *sync, int updates, int status)
{
    if( OPAL_LIKELY(OPAL_SUCCESS == status) ) {
        if( 0 != (OPAL_THREAD_ADD32(&sync->count, -updates)) ) {
            return;
        }
    } else {
        /* this is an error path so just use the atomic */
        opal_atomic_swap_32 (&sync->count, 0);
        sync->status = OPAL_ERROR;
    }
    WAIT_SYNC_SIGNAL(sync);
}

END_C_DECLS
