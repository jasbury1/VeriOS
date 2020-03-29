

/* Standard includes. */
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "esp_newlib.h"
#include "esp_compiler.h"

/* FreeRTOS includes. */
#include "verios.h"
#include "task.h"
#include "schedule.h"
#include "timers.h"
#include "StackMacros.h"
#include "portmacro.h"
#include "portmacro_priv.h"
#include "semphr.h"

/* Lint e961 and e750 are suppressed as a MISRA exception justified because the
MPU ports require MPU_WRAPPERS_INCLUDED_FROM_API_FILE to be defined for the
header files above, but not in this file, in order to generate the correct
privileged Vs unprivileged linkage and placement. */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750. */

/*
 * Defines the size, in bytes, of the stack allocated to the idle task.
 */
#define tskIDLE_STACK_SIZE	configIDLE_TASK_STACK_SIZE

/**
 * STATIC FUNCTION DECLARATIONS
 */

static void _OS_task_init_tcb(TCB_t *tcb, const char *task_name, TaskPrio_t prio, int stack_size, 
        OSBool_t is_static, int core_ID, int msg_queue_size);

static void _OS_task_init_stack(TCB_t *tcb, int stack_size, StackType_t *stack_alloc, 
        TaskFunc_t task_func, void *task_arg, TaskPrio_t prio);

static void _OS_task_delete_TLS(TCB_t *tcb);

static void _OS_task_delete_TCB(TCB_t *tcb)l

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

    _OS_task_init_tcb(task_tcb, task_name, prio, stack_size, OS_FALSE, core_ID, msg_queue_size);
    _OS_task_init_stack(task_tcb, stack_size, task_stack, task_func, task_arg, prio);
    _OS_task_make_ready(task_tcb, core_ID);

    return 0;
}

/**
 * 
 */
int OS_task_delete(TCB_t *tcb)
{
	int tcb_core_ID;
	int current_core = xPortGetCoreID();

	/* Cannot delete the idle task */
	if(tcb == OS_schedule_get_idle_tcb(tcb_core_ID)){
		return -1;
	}

    /* We will assume the current task is to be removed if the tcb is null */
    if(tcb == NULL){
        tcb = OS_schedule_get_current_TCB();
    }

    tcb_core_ID = tcb->core_ID;

    OS_schedule_remove_from_ready_list(tcb, tcb_core_ID);

    /* See if the task is ready to be deleted/freed now */
    if(tcb->task_state == OS_TASK_STATE_READY_TO_DELETE) {
        /* Delete local storage pointers */
        _OS_task_delete_TLS(tcb);
        _OS_task_delete_TCB(tcb);
    }
    return 0;
}

/**
 * Delete Thread-Local-Storage pointers if any exist for the given task
 */
static void _OS_task_delete_TLS(TCB_t *tcb)
{
    int i;
    for(i = 0; i < configNUM_THREAD_LOCAL_STORAGE_POINTERS; ++i){
        if (tcb->TLS_delete_callback_table[i] != NULL) {
			tcb->TLS_delete_callback_table[i](i, tcb->TLS_table[i]);
		}
    }
}

/**
 * Delete and free the TCB itself
 */
static void _OS_task_delete_TCB(TCB_t *tcb)
{
    /* If the port includes any specific cleanup */
    portCLEAN_UP_TCB(tcb);

    /* TODO: Clean up MPU settings . . . once I implement MPU settings . . . */
    /* TODO: Clean up anything else? Message Queues? */

    /* If the task is dynamically allocated */
    if(tcb->is_static == OS_FALSE) {
        /* Free the stack and TCB itself */
        vPortFreeAligned(tcb->stack_start);
		vPortFree(tcb)
    }
    /* Stack is statically allocated */
    else {
        /* TODO: just the stack or tcb static? or are both? Handle these cases */
    }
}

static void _OS_task_init_tcb(TCB_t *tcb, const char *task_name, TaskPrio_t prio, int stack_size, 
        OSBool_t is_static, int core_ID, int msg_queue_size)
{
    tcb->is_static = is_static;
    tcb->priority = prio;
    tcb->task_name = task_name;
    tcb->core_ID = core_ID;
    tcb->msg_queue_size = msg_queue_size;
    tcb->mutexes_held = 0;
    tcb->delay_wakeup_time = 0;
    
    /* Initialize list parameters to null for now */
    tcb->next_ptr = NULL;
    tcb->prev_ptr = NULL;
}

static void _OS_task_init_stack(TCB_t *tcb, int stack_size, StackType_t *stack_alloc, 
        TaskFunc_t task_func, void *task_arg, TaskPrio_t prio)
{
    StackType_t stack_top;
    StackType_t stack_end;
    OSBool_t run_privileged;

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

