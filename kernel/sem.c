/* Standard includes. */
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"

/* FreeRTOS includes. */
#include "FreeRTOS_old.h"
#include "verios.h"
#include "task.h"
#include "schedule.h"
#include "verios_util.h"
#include "sem.h"

/*******************************************************************************
* Semaphore Create (API FUNCTION)
*
*   sem_ptr = A pointer to a Sem_t reference for the semaphore we will create
*   value = The initial value of the semaphore
* 
* PURPOSE :
*
*   This is the API call for creating a semaphore
* 
* RETURN :
* 
*   Returns an error code or 0 (OS_NO_ERROR) if no error occurred 
*
* NOTES: 
*******************************************************************************/

int OS_sem_create(Sem_t * sem_ptr, int value)
{
    Semaphore_t *sem;

    /* Allocate the semaphore */
    sem = malloc(sizeof(Semaphore_t));
    if(sem == NULL) {
        return OS_ERROR_SEM_ALLOC;
    }

    /* Initialize the semapore fields */
    sem->value = value;
    _OS_list_header_init(&(sem->waiters));
    vPortCPUInitializeMutex(&sem->mux);

    /* Set the pointer to point to the semaphore allocation */
    *sem_ptr = (void *)sem;
    
    return OS_NO_ERROR;
}

/*******************************************************************************
* Semaphore Delete (API FUNCTION)
*
*   sem_ptr = A pointer to the Sem_t reference for the semaphore to delete  
* 
* PURPOSE : 
*  
*   Destroy a semaphore and free its associated data
* 
* RETURN :
*   
*   Returns an error code or 0 (OS_NO_ERROR) if no error occured
*
* NOTES: 
*******************************************************************************/

int OS_sem_delete(Sem_t *sem)
{
    Semaphore_t ** sem_ptr = (Semaphore_t **)sem;

    if(*sem_ptr == NULL) {
        return OS_ERROR_INVALID_SEM;
    }

    portENTER_CRITICAL(&((*sem_ptr)->mux));
    OS_schedule_waitlist_empty(&((*sem_ptr)->waiters));
    portEXIT_CRITICAL(&((*sem_ptr)->mux));
    
    free(*sem_ptr);
    sem = NULL;
    return OS_NO_ERROR;
}

/*******************************************************************************
* Semaphore Take (API FUNCTION)
*
*   semaphore = The semaphore to take (down operation)
* 
* PURPOSE : 
*
*   Perform a take ('down', 'wait', etc...) on a semaphore. This operation is
*   blocking if the semaphore value is currently 0 
* 
* RETURN :
*
*   Return an error message or 0 (OS_NO_ERROR) if no error occured   
*
* NOTES: 
*
*******************************************************************************/

int OS_sem_take(Sem_t semaphore)
{
    TCB_t *tcb = OS_schedule_get_current_tcb();
    Semaphore_t *sem = (struct OSSemaphore *)semaphore;

    if(sem == NULL) {
        return OS_ERROR_INVALID_SEM;
    }

    while (OS_TRUE) {
        portENTER_CRITICAL(&(sem->mux));
        
        /* Add the message if there is room on the queue */
        if (sem->value > 0)
        {
            sem->value--;

            portEXIT_CRITICAL(&(sem->mux));
            tcb->is_blocked = OS_FALSE;
            return OS_NO_ERROR;
        }

        /* We were unable to take the semaphore */

        /* Add the task to a waitlist so that it can be woken up once the sem is released */
        _OS_waitlist_append(tcb, &(sem->waiters));
        tcb->is_blocked = OS_TRUE;

        portEXIT_CRITICAL(&(sem->mux));

        OS_schedule_suspend_task(tcb);

    }
    tcb->is_blocked = OS_FALSE;
    return OS_NO_ERROR;
}

/*******************************************************************************
* Semaphore Release (API FUNCTION)
*
*   semaphore = The semaphore to release (up operation)
* 
* PURPOSE : 
*
*   Perform a release ('up', 'post', etc...) on a semaphore. This operation is
*   non-blocking and will wake up the next task waiting on the semaphore if there
*   is one
* 
* RETURN :
*
*   Return an error code or 0 (OS_NO_ERROR) if no error occured
*
* NOTES: 
*
*******************************************************************************/

int OS_sem_release(Sem_t semaphore)
{
    TCB_t *woken_task = NULL;
    struct OSSemaphore *sem = (struct OSSemaphore *)semaphore;

    if(sem == NULL) {
        return OS_ERROR_INVALID_SEM;
    }

    portENTER_CRITICAL(&(sem->mux));
    sem->value++;
    if(sem->waiters.num_tasks != 0) {
        woken_task = _OS_waitlist_pop_head(&(sem->waiters));
    }
    portEXIT_CRITICAL(&(sem->mux));
    if(woken_task != NULL) {
        OS_schedule_resume_task(woken_task);
    }
    
    return OS_NO_ERROR;
}

/*******************************************************************************
* Mutex Create (API FUNCTION)
*
*   mux_ptr = A pointer to a Mux_t reference for the mutex we will create
* 
* PURPOSE :
*
*   This is the API call for creating a mutex
* 
* RETURN :
* 
*   Returns an error code or 0 (OS_NO_ERROR) if no error occurred 
*
* NOTES: 
*******************************************************************************/

inline int OS_mux_create(Mux_t *mux_ptr)
{
    return OS_sem_create(mux_ptr, 1);
}

/*******************************************************************************
* Mutex Delete (API FUNCTION)
*
*   mux_ptr = A pointer to the Mux_t reference for the mutex to delete  
* 
* PURPOSE : 
*  
*   Destroy a mutex and free its associated data
* 
* RETURN :
*   
*   Returns an error code or 0 (OS_NO_ERROR) if no error occured
*
* NOTES: 
*******************************************************************************/

inline int OS_mux_delete(Mux_t *mux_ptr)
{
    return OS_sem_delete(mux_ptr);
}

/*******************************************************************************
* Mutex Take (API FUNCTION)
*
*   mutex = The mutex to take (down operation)
* 
* PURPOSE : 
*
*   Perform a take ('down', 'wait', etc...) on a mutex. This operation is
*   blocking if the semaphore value is currently 0 
* 
* RETURN :
*
*   Return an error message or 0 (OS_NO_ERROR) if no error occured   
*
* NOTES: 
*
*******************************************************************************/

inline int OS_mux_take(Mux_t mutex){
    return OS_sem_take(mutex);
}

/*******************************************************************************
* Mutex Release (API FUNCTION)
*
*   mutex = The mutex to release (up operation)
* 
* PURPOSE : 
*
*   Perform a release ('up', 'post', etc...) on a mutex. This operation is
*   non-blocking and will wake up the next task waiting on the mtuex if there
*   is one
* 
* RETURN :
*
*   Return an error code or 0 (OS_NO_ERROR) if no error occured
*
* NOTES: 
*
*******************************************************************************/
inline int OS_mux_release(Mux_t mutex){
    return OS_sem_release(mutex);
}
