#ifndef OS_TASK_H
#define OS_TASK_H


#include <limits.h>
#include <stdint.h>

#include "freertos/portmacro.h"
#include "verios.h"

#define CORE_NO_AFFINITY -1

#define OS_STACK_FILL_BYTE	( 0xa5U )

#define OS_IDLE_STACK_SIZE configIDLE_TASK_STACK_SIZE
#define OS_IDLE_PRIORITY (uint8_t)0
#define OS_IDLE_NAME ((const char* const)"IDLE")

/* Specity the size of the integer for a task priority */
typedef uint8_t TaskPrio_t;

/* The function pointer representing a task function */
typedef void (*TaskFunc_t)( void * );

/* If we ever want to pass TCBs around anonymously, cast as TaskHandle_t */
typedef void * TaskHandle_t;

typedef struct OSTaskControlBlock TCB_t;

typedef enum {
    OS_TASK_STATE_READY,
    OS_TASK_STATE_DELAYED,
    /* TODO add more */
    OS_TASK_STATE_PENDING_DELETION,
    OS_TASK_STATE_READY_TO_DELETE
} OSTaskState_t;

typedef void *TLSPtr_t;
typedef void (*TLSPtrDeleteCallback_t)(int, void *);

/**
 * The Task Control Block
 * Kernel bookkeeping on each task created
 * 
 * Should NEVER be used by user code
 */
struct OSTaskControlBlock
{
    /* Pointer to last element pushed to stack */
    volatile StackType_t *stack_top;

    /* Set to OS_TRUE if the task was statically allocated and doesn't require freeing */
    OSBool_t is_static;

    /* The core being used for multi-core systems */
    int core_ID;

    TaskPrio_t priority;
    StackType_t *stack_start;
    StackType_t *stack_end;
    int stack_size;
    const char * task_name;

    /* IPC data */
    int msg_queue_size;

    int mutexes_held;

    /* List data */
    TCB_t *next_ptr;
    TCB_t *prev_ptr;

    /* Thread local storage pointers*/
    TLSPtr_t TLS_table[configNUM_THREAD_LOCAL_STORAGE_POINTERS];
    TLSPtrDeleteCallback_t TLS_delete_callback_table[configNUM_THREAD_LOCAL_STORAGE_POINTERS];

    /* State variables */
    OSTaskState_t task_state;

    /* Time to wake this task up if it has been delayed */
    TickType_t delay_wakeup_time;

};

/**
 * FUNCTION HEADERS
 */

int OS_task_create(TaskFunc_t task_func, void *task_arg, const char * const task_name, 
            TaskPrio_t prio, int stack_size, int msg_queue_size, int core_ID, TCB_t *task_tcb);

int OS_task_delete(TCB_t *tcb);

#endif /* OS_TASK_H */