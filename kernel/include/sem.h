#ifndef OS_SEM_H
#define OS_SEM_H

#include "verios.h"

/*******************************************************************************
* MACROS
*******************************************************************************/

/*******************************************************************************
* TYPEDEFS AND DATA STRUCTURES
*******************************************************************************/

/* The handle for API usage */
typedef void * Sem_t;
typedef void * Mux_t;

/* The main structure for a semaphore */
typedef struct OSSemaphore {
    int value;
    WaitList_t waiters;

    portMUX_TYPE mux;
} Semaphore_t;

/*******************************************************************************
* FUNCTION HEADERS
*******************************************************************************/

int OS_sem_create(Sem_t *sem_ptr, int value);

int OS_sem_delete(Sem_t *sem_ptr);

int OS_sem_take(Sem_t semaphore);

int OS_sem_release(Sem_t semaphore);

inline int OS_mux_create(Mux_t *mux_ptr);

inline int OS_mux_delete(Mux_t *mux_ptr);

inline int OS_mux_take(Mux_t mutex);

inline int OS_mux_release(Mux_t mutex);

#endif /* OS_SEM_H */