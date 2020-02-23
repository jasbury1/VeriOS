/**
 * 
 * HEADER GOES HERE- make it look cool later
 *
 *
 *
 *
 */

/* Standard includes */
#include <stdlib.h>

/* OS specific includes */
#include "include/task.h"
#include "../cpu/xtensa/portmacro.h"

/**
 * The Task Control Block
 * Kernel bookkeeping on each task created
 * 
 * Should NEVER be used by user code
 */
typedef struct OSTaskControlBlock
{

} TCB_t;

/**
 * OS_task_create
 * Creates a new task by allocating the stack... etc.
 * 
 * task_func - The function representing a new task
 * task_arg - The argument passed to the task function
 * task_name - The name of the task. For debugging only. Can be null
 * prop - The priority of the task (0-255)
 * stack_size - Size of the stack. Measured in *WORDS*
 * msg_queue_size - ipc message queue size for this task. Can be 0 or negative for no queue. Measured in messages
 * task_tcb - area to allocate tcb. Can be null
 * 
 * Return - Error code for task creation, or 0 if no error occured
 */
int OS_task_create(TaskFunc_t task_func, void *task_arg, const char *task_name, 
            TaskPrio_t prio, int stack_size, int msg_queue_size, TCB_t *task_tcb)
{
    /* Priority 0 is reserved for the Idle task */
    if(prio == (TaskPrio_t)0) {
        /* TODO: Typechecking to see if this is the idle TCB. IF it is, dont error */
        return -1;
        /* TODO: Better error code */
    }
    
    /* TODO: We assume the stack grows downwards only. Option later on? */

    /* Allocate Stack first so that TCB does not interact with stack memory */


}
/*
 * The static counterpart to OS_task_create
 */
void OS_task_create_s();

void OS_task_destroy();

void OS_task_change_priority();

void OS_task_suspend();

void OS_task_resume();


