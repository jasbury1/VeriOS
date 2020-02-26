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
#include "include/verios.h"
#include "../cpu/xtensa/portmacro.h"

/**
 * The Task Control Block
 * Kernel bookkeeping on each task created
 * 
 * Should NEVER be used by user code
 */
typedef struct OSTaskControlBlock
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
 * core_ID - The core being used for multicore systems
 * task_tcb - area to allocate tcb. Can be null
 * 
 * Return - Error code for task creation, or 0 if no error occured
 */
int OS_task_create(TaskFunc_t task_func, void *task_arg, const char *task_name, 
            TaskPrio_t prio, int stack_size, int msg_queue_size, int core_ID, TCB_t *task_tcb)
{
    int i;
    StackType_t *task_stack = null;

    /* Priority 0 is reserved for the Idle task */
    if(prio == (TaskPrio_t)0) {
        /* TODO: Typechecking to see if this is the idle TCB. IF it is, dont error */
        return -1;
        /* TODO: Better error code */
    }
    
    /* TODO: We assume the stack grows downwards only. Option later on? */

    /* Allocate Stack first so that TCB does not interact with stack memory */
	task_stack = ( StackType_t * ) pvPortMallocStackMem( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ) );
    if(task_stack == null) {
        return -1;
    }

    task_tcb = ( TCB_t * ) pvPortMallocTcbMem( sizeof( TCB_t ) );
    if(task_tcb == null){
        /* Allocating TCB failed. Free the stack and error out */
        vPortFree(task_stack);
        return -1;
    }
    
    /* Handle message queue */
    if(msg_queue_size > 0) {
        /* TODO */
    }

    _OS_task_init_tcb(task_tcb, task_name, prio, stack_size, OS_FALSE, core_ID);
    _OS_task_init_stack(task_tcb, stack_size, task_stack, task_func, task_arg, prio);
    _OS_task_make_ready(task_tcb);

    return 0;
}

/*
 * The static counterpart to OS_task_create
 */
int OS_task_create_s();

static void _OS_task_init_tcb(TCB_t *tcb, const char *task_name, TaskPrio_t prio, int stack_size, 
        uint8_t is_static, int core_ID, int msg_queue_size)
{
    tcb->is_static = is_static;
    tcb->priority = prio;
    tcb->task_name = task_name;
    tcb->core_ID = core_ID;
    tcb->msg_queue_size = msg_queue_size;
}

static void _OS_task_init_stack(TCB_t *tcb, int stack_size, StackType_t *stack_alloc, 
        TaskFunc_t task_func, void *task_arg, TaskPrio_t prio)
{
    StackType_t stack_top;
    StackType_t stack_end;
    uint8_t run_privileged;

    #if( portUSING_MPU_WRAPPERS == 1) {
        if((prio & OS_PRIVILEGE_BIT) != 0U) {
            run_privileged = OS_TRUE;
        } else {
            run_privileged = OS_FALSE;
        }
        prio &= ~OS_PRIVILEGE_BIT;
    }
    #endif /* portUSING_MPU_WRAPPERS */

    tcb->stack_start = stack_alloc;
    
    /* Zero out the stack. Debugging bits can replace the 0's later */
    memset(tcb->stack_start, (int)0x00, (size_t)(stack_size * sizeof(StackType_t)));

    /* TODO: Again here we assume the stack grows downwards */
    /* TODO: Stolen from FreeRTOS */
    /* TODO: I want to make this a macro in the future? */
    stack_top = tcb->stack_start + (stack_size - (uint32_t) 1);
    stack_top = (Stacktype_t *)(((portPOINTER_SIZE_TYPE) stack_top) & ( ~((portPOINTER_SIZE_TYPE) portBYTE_ALIGNMENT_MASK)));
    tcb->stack_end = stack_top;

    #if( portUsing_MPU_WRAPPERS == 1) {
        tcb->stack_top = pxPortInitialiseStack(stack_top, task_func, task_arg, run_privileged);
    }
    #else {
        tcb->stack_top = pxPortInitialiseStack(stack_top, task_func, task_arg);
    }
    #endif /* portUsing_MPU_WRAPPERS */

}

static void _OS_task_make_ready(TCB_t tcb, int core_ID)
{

}

void OS_task_destroy();

void OS_task_change_priority();

void OS_task_suspend();

void OS_task_resume();


