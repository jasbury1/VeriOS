#ifndef OS_TASK_H
#define OS_TASK_H


#include <limits.h>
#include <stdint.h>

#include "list.h"
#include "freertos/portmacro.h"

#define CORE_NO_AFFINITY -1

#define OS_IDLE_STACK_SIZE configIDLE_TASK_SIZE

/* Specity the size of the integer for a task priority */
typedef uint8_t TaskPrio_t;

/* The function pointer representing a task function */
/* TODO: Should be moved to a better header file */
typedef void (*TaskFunc_t)( void * );

typedef struct OSTaskControlBlock TCB_t;

typedef enum {
    OS_TASK_STATE_READY,
    OS_TASK_STATE_DELAYED,
    /* TODO add more */
    OS_TASK_STATE_PENDING_DELETION,
    OS_TASK_STATE_READY_TO_DELETE
} OSTaskState_t;

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
    uint8_t is_static;

    /* The core being used for multi-core systems */
    int core_ID;

    TaskPrio_t priority;
    StackType_t stack_start;
    StackType_t stack_end;
    int stack_size;
    char * task_name;

    /* IPC data */
    int msg_queue_size;

    int mutexes_held;

    /* List data */
    TCB_t *next_ptr;
    TCB_t *prev_ptr;

    /* State variables */
    OSTaskState_t task_state;

};

/**
 * FUNCTION HEADERS
 */

int OS_task_create(TaskFunc_t task_func, void *task_arg, const char *task_name, 
            TaskPrio_t prio, int stack_size, int msg_queue_size, int core_ID, TCB_t *task_tcb);

int OS_task_delete(TCB_t *tcb);

#endif /* OS_TASK_H */



