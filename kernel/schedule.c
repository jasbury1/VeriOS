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
#include "FreeRTOS_old.h"
#include "verios.h"
#include "task.h"
#include "schedule.h"
#include "verios_util.h"
#include "timers.h"
#include "list.h"
#include "StackMacros.h"
#include "portmacro.h"
#include "portmacro_priv.h"
#include "semphr.h"

/* MISRA exception justified because the MPU ports require 
MPU_WRAPPERS_INCLUDED_FROM_API_FILE to be defined for the header files above, 
but not in this file, in order to generate the correct privileged Vs unprivileged 
linkage and placement. */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750. */

/* The following is from FreeRTOS and should be removed at some point */
#define taskEVENT_LIST_ITEM_VALUE_IN_USE	0x80000000UL


/*******************************************************************************
* SCHEDULER CRITICAL STATE VARIABLES
*******************************************************************************/

PRIVILEGED_DATA static volatile OSBool_t OS_scheduler_running = OS_FALSE;
PRIVILEGED_DATA static volatile TickType_t OS_tick_counter = (TickType_t)0;
PRIVILEGED_DATA static volatile TickType_t OS_tick_overflow_counter = (TickType_t)0;
PRIVILEGED_DATA static volatile TickType_t OS_pending_ticks = (TickType_t)0;

/* Keeps track of the TCBs that are currently running on the processor */
PRIVILEGED_DATA TCB_t * volatile OS_current_TCB[portNUM_PROCESSORS] = { NULL };

/* The IDLE TCBs for each processor */
PRIVILEGED_DATA static TCB_t * OS_idle_tcb_list[portNUM_PROCESSORS] = { NULL };

/* A counter for the number of tasks currently ready (not deleted) */
PRIVILEGED_DATA static volatile unsigned int OS_num_tasks = 0;

/* A counter for all tasks that have been deleted or pending deletion */
PRIVILEGED_DATA static volatile unsigned int OS_num_tasks_deleted = 0;

/* A counter for tasks that have been delayed and placed on a list */
PRIVILEGED_DATA static volatile unsigned int OS_num_tasks_delayed = 0;

/* A counter for the time to unblock the next task that is blocked */
PRIVILEGED_DATA static volatile TickType_t OS_next_task_unblock_time = portMAX_DELAY;

/* Keep track of if schedulers are suspended (OS_TRUE) or not (OS_FALSE) */
PRIVILEGED_DATA static volatile OSBool_t OS_suspended_schedulers_list[portNUM_PROCESSORS] = {OS_FALSE};

/* Keep track of if yields are pending (OS_TRUE) or not (OS_FALSE) on a given processor */
PRIVILEGED_DATA static volatile OSBool_t OS_yield_pending_list[portNUM_PROCESSORS] = {OS_FALSE};

/* Keep track of if a core is switching contexts (OS_TRUE) or not (OS_FALSE) */
PRIVILEGED_DATA static volatile OSBool_t OS_switching_context_list[portNUM_PROCESSORS]  = {OS_FALSE};

/* One mutex for use in scheduler actions */
PRIVILEGED_DATA static portMUX_TYPE OS_schedule_mutex = portMUX_INITIALIZER_UNLOCKED;

/*******************************************************************************
* SCHEDULER CRITICAL DATA STRUCTURES
*******************************************************************************/

/**
 * Bitmap of priorities in use.
 * Broken up into an array of 8 bit integers where each bit represents if a priority is in use
 * The map is designed backwards. 
 * The first bit corresponds to the highest priority (highest number)
 * The last bit is priority 0 (reserved for Idle)
 */
static volatile uint8_t OS_ready_priorities_map[OS_PRIO_MAP_SIZE] = {(uint8_t)0};

/**
 * The Ready list of all tasks ready to be run
 * Each index corresponds to a priority. Index 0 is priority 0 (reserved for Idle)
 */
static volatile ReadyList_t OS_ready_list[OS_MAX_PRIORITIES] = {{0, NULL, NULL}};

/**
 * The list of tasks that were scheduled while the scheduler was suspended
 */

static volatile ReadyList_t OS_pending_ready_list[portNUM_PROCESSORS] = {{0, NULL, NULL}};

/**
 * A list of tasks pending deletion (memory not yet freed).
 * Deletion should be carried out by IDLE for these tasks
 */
static DeletionList_t OS_deletion_pending_list = {0, NULL, NULL};

/**
 * The lists of delayed tasks
 * Index 0 is the regular list, index 1 is a list for overflowed delays.
 * Both indece's lists are sorted by ascending wake-up time
 */
static volatile DelayedList_t OS_delayed_list = {0, NULL, NULL};

/**
 * A list of tasks that have been suspended.
 * This list is unordered. Inserting adds to the end
 */
static SuspendedList_t OS_suspended_list = {0, NULL, NULL};

/*******************************************************************************
* STATIC FUNCTION DECLARATIONS
*******************************************************************************/

static void _OS_bitmap_reset_prios(void);

static int _OS_schedule_get_highest_prio(void);

static void _OS_bitmap_add_prio(TaskPrio_t new_prio);

static void _OS_bitmap_remove_prio(TaskPrio_t old_prio);

static void _OS_ready_list_init(void);

static void _OS_ready_list_insert(TCB_t *new_tcb);

static void _OS_ready_list_remove(TCB_t *old_tcb);

static void _OS_deletion_pending_list_insert(TCB_t *tcb);

static void _OS_deletion_pending_list_empty();

static void _OS_delayed_list_insert(TCB_t *tcb);

static void _OS_delayed_list_remove(TCB_t *tcb);

static void _OS_suspended_list_insert(TCB_t *tcb);

static void _OS_suspended_list_remove(TCB_t *tcb);

static OSBool_t _OS_delayed_list_wakeup_next_task(void);

static void _OS_delayed_tasks_cycle_overflow(void);

static void _OS_pending_ready_list_schedule_next_task(void);

static void _OS_pending_ready_list_insert(TCB_t *tcb, int core_ID);

static void _OS_update_next_task_unblock_time(void);

static void _OS_schedule_yield_other_core(int core_ID, TaskPrio_t prio );

/*******************************************************************************
* IDLE TASK
*******************************************************************************/

void OS_IDLE_TASK(void * idle_task_param)
{
    for(;;) {
        if(OS_deletion_pending_list.num_tasks != 0) {
            /* _OS_deletion_pending_list_empty(); */
        }
        extern void vApplicationIdleHook(void);
        vApplicationIdleHook();
    }
}

/*******************************************************************************
* OS Schedule Start
* 
* PURPOSE :
*
*   Starts the scheduler. Creates all idle tasks and creates the timer task.
*   Starts the OS clock at 0
* 
* RETURN : 
*
*   Return an error code or 0 (OS_NO_ERROR) if no error occured
*
* NOTES:
*
*   This function will not return as long as the scheduler is running!
*******************************************************************************/

int OS_schedule_start(void)
{
    int i;
    int ret_val;

    for(i = 0; i < portNUM_PROCESSORS; ++i) {
        ret_val = OS_task_create(OS_IDLE_TASK, NULL, OS_IDLE_NAME, 0, OS_IDLE_STACK_SIZE, 
                                    0, i, (void **)(&OS_idle_tcb_list[i]));
        if(ret_val != OS_NO_ERROR){
            return ret_val;
        }
    }
    /* TODO: we use FreeRTOS timer APIs. change to our own API in the future */
    ret_val = xTimerCreateTimerTask();
    if(ret_val != 1){
        return ret_val;
    }

    /* Set up the scheduler tick counter */
    portDISABLE_INTERRUPTS();
    OS_tick_counter = ( TickType_t ) 0U;
    portCONFIGURE_TIMER_FOR_RUN_TIME_STATS();
    OS_scheduler_running = OS_TRUE;

    if (xPortStartScheduler() != OS_FALSE) {
        /* Will Not reach here. Function never returns */
    }
    else {
        /* Should only reach here if a task calls OS_schedule_stop */
    }
    return OS_NO_ERROR;
}

/*******************************************************************************
* OS Schedule Stop
*
* PURPOSE :
*
*   Stops the scheduler completely on all processors
* 
* RETURN : 
*
* NOTES: 
*
*   The port layer must ensure interrupt enable bit is left in the correct state
*******************************************************************************/

void OS_schedule_stop()
{
    /* Stop the scheduler interrupts and call the portable scheduler end
	routine so the original ISRs can be restored if necessary.*/
    portDISABLE_INTERRUPTS();
    OS_scheduler_running = OS_FALSE;
    vPortEndScheduler();
}

/*******************************************************************************
* OS Schedule Suspend
*
* PURPOSE : 
*
*   Suspend the scheduler on the caller's core with intent to resume it later.
*   No tasks will be scheduled or run on this processor.
* 
* RETURN : 
*
* NOTES: 
*   This is in regards to the scheduler itself. Does not suspend individual
*   suspended tasks.
*******************************************************************************/

void OS_schedule_suspend(void)
{
    unsigned state;
    state = portENTER_CRITICAL_NESTED();
    /* Increment rather than just set to OS_TRUE since suspend calls can be nested */
    OS_suspended_schedulers_list[xPortGetCoreID()]++;
    portEXIT_CRITICAL_NESTED(state);
}

/*******************************************************************************
* OS Schedule Resume
* 
* PURPOSE :
*
*   Resumes the scheduler for the caller's core after a call to OS_schedule_suspend
* 
* RETURN :
*
*   Returns OS_TRUE if a context switch occured, or OS_FALSE otherwise
*
* NOTES:
*
*   This is in regards to the scheduler itself. Does not resume individual
*   suspended tasks.
*******************************************************************************/

OSBool_t OS_schedule_resume(void)
{
    OSBool_t yield_occured = OS_FALSE;
    assert(OS_suspended_schedulers_list[xPortGetCoreID()] >= OS_TRUE);

    portENTER_CRITICAL(&OS_schedule_mutex);

    OS_suspended_schedulers_list[xPortGetCoreID()]--;
    
    /* Nothing to do if we are still in nested suspension or have no tasks */
    if(OS_suspended_schedulers_list[xPortGetCoreID()] != OS_FALSE ||
       OS_num_tasks == 0) {
        portEXIT_CRITICAL(&OS_schedule_mutex); 
        return yield_occured;
    }

    /* Ready all of the tasks from the pending ready list */
    while(OS_pending_ready_list[xPortGetCoreID()].num_tasks > 0) {
        _OS_pending_ready_list_schedule_next_task();
    }

    /* Get up to date with all the ticks that have passed */
	while( OS_pending_ticks > 0) {
		if( OS_schedule_process_tick() == OS_TRUE) {
			OS_yield_pending_list[ xPortGetCoreID() ] = OS_TRUE;
		}
		--OS_pending_ticks;
	}

    /* Yield if we missed a necessary yield while suspended */
    if( OS_yield_pending_list[xPortGetCoreID()] == OS_TRUE ) {
		yield_occured = OS_TRUE;
        portYIELD_WITHIN_API();
	}

    portEXIT_CRITICAL(&OS_schedule_mutex);
    return yield_occured;
}

/*******************************************************************************
* OS Schedule Switch Context
* 
* PURPOSE : 
*
*   Force a context switch on the caller's core. This will cycle round robin
*   positions and run the task with the highest priority that has not run
*   for the longest amount of time.
* 
* RETURN : 
*
* NOTES:
*   
*   If the scheduler is suspended, it will save the context switch for once
*   it is resumed again.
*******************************************************************************/

void OS_schedule_switch_context(void)
{
    int state;
    int core_ID;
    TaskPrio_t ready_list_index;
    TCB_t *tcb_to_run = NULL;
    TCB_t *tcb_swapped_out = NULL;
    TCB_t *temp = NULL;

    state = portENTER_CRITICAL_NESTED();

    core_ID = xPortGetCoreID();

    /* Do not switch context if we are suspended! */
    if(OS_suspended_schedulers_list[core_ID] != OS_FALSE){
        OS_yield_pending_list[core_ID] = OS_TRUE;
        portEXIT_CRITICAL_NESTED(state);
        return;
    }

    OS_yield_pending_list[core_ID] = OS_FALSE;
    OS_switching_context_list[core_ID] = OS_TRUE;

    /* TODO: check for stack overflow one day */

    vPortCPUAcquireMutex(&OS_schedule_mutex);

    tcb_swapped_out = OS_current_TCB[core_ID];
    if(tcb_swapped_out->task_state == OS_TASK_STATE_RUNNING){
        tcb_swapped_out->task_state = OS_TASK_STATE_READY;
    }

    /* Cycle the round robin position of the task that finished running */
    if(tcb_swapped_out->task_state == OS_TASK_STATE_READY && 
            tcb_swapped_out->priority != 0) {
        _OS_ready_list_remove(tcb_swapped_out);
        /* Inserting always inserts in the back of the respective ready list */
        _OS_ready_list_insert(tcb_swapped_out);
    }

    ready_list_index = _OS_schedule_get_highest_prio();
    temp = OS_ready_list[ready_list_index].head_ptr;

    /* Find the next highest priority task we can run on this core */
    while(ready_list_index > 0 && tcb_to_run == NULL) {

        /* No valid task to run at this priority */
        if(temp == NULL) {
            --ready_list_index;
            temp = OS_ready_list[ready_list_index].head_ptr;
        }
        /* We found a valid task to run */
        else if(temp->task_state != OS_TASK_STATE_RUNNING &&
                (temp->core_ID == core_ID || temp->core_ID == CORE_NO_AFFINITY)){
            tcb_to_run = temp;
        }
        /* Try the next round robin task at this priority */
        else {
            temp = temp->next_ptr;
        }
    }

    /* See if we need to run IDLE */
    if(tcb_to_run == NULL){
        tcb_to_run = OS_idle_tcb_list[core_ID];
    }

    tcb_to_run->task_state = OS_TASK_STATE_RUNNING;
    OS_current_TCB[core_ID] = tcb_to_run;

    OS_switching_context_list[core_ID] = OS_FALSE;
    vPortCPUReleaseMutex(&OS_schedule_mutex);
    portEXIT_CRITICAL_NESTED(state);
    return;
}

/*******************************************************************************
* OS Schedule Add Task
*
*   new_tcb = A pointer to the tcb to get added to the scheduler
* 
* PURPOSE :
*
*   A function for adding a task to the scheduler. This does the work of
*   adding to the ready list and finding an appropriate core if the task has
*   no affinity.
* 
* RETURN : 
*
*   Returns an error code or 0 (OS_NO_ERROR) if no error occured
*
* NOTES:
*
*   This should rarely need to get called by application code. It is automatically
*   called by OS_task_create
*******************************************************************************/

int OS_schedule_add_task(TCB_t *new_tcb)
{
    int i;
    int core_ID;

    if(new_tcb->priority >= OS_MAX_PRIORITIES || new_tcb->priority < 0) {
        return OS_ERROR_INVALID_PRIO;
    }

    portENTER_CRITICAL(&OS_schedule_mutex);

    core_ID = new_tcb->core_ID;
    new_tcb->task_state = OS_TASK_STATE_READY;
    
    /* If there is no affinity with which core it is placed on */
    if(core_ID == CORE_NO_AFFINITY) {
        /* Place it on the single existing core if we don't have multiple cores */
        core_ID = portNUM_PROCESSORS == 1 ? 0 : core_ID;

        /* If there is a core that can run it now, place it there */
        for(i = 0; i < portNUM_PROCESSORS; ++i) {
            if(OS_current_TCB[i] == NULL || 
                    OS_current_TCB[i]-> priority < new_tcb->priority) {
                core_ID = i;
                break;
            }
        }
        /* If we still haven't found a core, place it on the current one */
        if(core_ID == CORE_NO_AFFINITY) {
            core_ID = xPortGetCoreID();
        }
    }

    /* If nothing is running on this core then put our task there */
    if(OS_current_TCB[core_ID] == NULL ) {
        OS_current_TCB[core_ID] = new_tcb;
        new_tcb->task_state = OS_TASK_STATE_RUNNING;
    }

    /* Make this task the current if its priority is the highest 
    priority and the scheduler isn't running */
    else if(OS_scheduler_running == OS_FALSE){
        if(OS_current_TCB[core_ID]->priority <= new_tcb->priority) {
            OS_current_TCB[core_ID]->task_state = OS_TASK_STATE_READY;
            OS_current_TCB[core_ID] = new_tcb;
            new_tcb->task_state = OS_TASK_STATE_RUNNING;
        }
    }
    
    /* Increase the task count and update the ready list */
    ++OS_num_tasks;
    _OS_ready_list_insert(new_tcb);

    /* Nothing else to do it the schedule is not running */
    if(OS_scheduler_running == OS_FALSE) {
        portEXIT_CRITICAL(&OS_schedule_mutex);
        return OS_NO_ERROR;
    }

	/* Scheduler is running. Check to see if we should run the task now */
	if(OS_current_TCB[core_ID]->priority < new_tcb->priority) {
		if( core_ID == xPortGetCoreID() )
		{
			portYIELD_WITHIN_API();
		}
		else {
            /* See if we need to interrupt the other core to run the task now */
            _OS_schedule_yield_other_core(core_ID, new_tcb->priority);
		}
	}

    portEXIT_CRITICAL(&OS_schedule_mutex);
    return OS_NO_ERROR;
}

/*******************************************************************************
* OS Schedule Remove Task
*
*   old_tcb = A pointer to the tcb to be removed from scheduling
* 
* PURPOSE : 
*
*   This function permanently removes a tcb from the scheduler and updates the
*   ready list or any other lists the task may be a part of. 
* 
* RETURN : 
*
*   Return an error code or 0 (OS_NO_ERROR) if no error occured
*
* NOTES: 
*
*   User code should use OS_task_delete instead of this function directly
*******************************************************************************/

int OS_schedule_remove_task(TCB_t *old_tcb)
{
    OSBool_t ready_to_delete = OS_TRUE;
    int i;

    portENTER_CRITICAL(&OS_schedule_mutex);
    
    assert(old_tcb != NULL);

    /* Cannot remove the idle task */
	if(old_tcb->priority == OS_IDLE_PRIORITY){
        portEXIT_CRITICAL(&OS_schedule_mutex);
        return OS_ERROR_IDLE_DELETE;
	}

    /* Remove from any state lists */
    switch(old_tcb->task_state) {
        case OS_TASK_STATE_RUNNING:
            _OS_ready_list_remove(old_tcb);
            break;
        case OS_TASK_STATE_READY:
            _OS_ready_list_remove(old_tcb);
            break;
        case OS_TASK_STATE_DELAYED:
            _OS_delayed_list_remove(old_tcb);
            break;
        case OS_TASK_STATE_SUSPENDED:
            _OS_suspended_list_remove(old_tcb);
            break;
        case OS_TASK_STATE_PENDING_DELETION:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_DOUBLE_DELETE;
        case OS_TASK_STATE_READY_TO_DELETE:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_DOUBLE_DELETE;
        default:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_INVALID_TSK_STATE;
    }
        
    /* Update task counters */
    OS_num_tasks--;
    OS_num_tasks_deleted++;

    /* Idle must delete this task if it is not on this core */
    if(old_tcb->core_ID != xPortGetCoreID()){
        ready_to_delete = OS_FALSE;
    }
    /* Idle must delete this task if it is running on any core */
    if(old_tcb->task_state == OS_TASK_STATE_RUNNING){
        ready_to_delete = OS_FALSE;
    }

    if(ready_to_delete == OS_TRUE){
        old_tcb->task_state = OS_TASK_STATE_READY_TO_DELETE;
    }
    else {
        old_tcb->task_state = OS_TASK_STATE_PENDING_DELETION;
        _OS_deletion_pending_list_insert(old_tcb);
    }

    /* Necessary since we use FreeRTOS events lists (for now) */
    if( listLIST_ITEM_CONTAINER( &( old_tcb->xEventListItem ) ) != NULL ){
		( void ) uxListRemove( &( old_tcb->xEventListItem ) );
    }

    /* Force a reschedule if the conditions require it */
    if(OS_scheduler_running == OS_TRUE){
        /* If the task is running on this core */
        if(old_tcb == OS_current_TCB[xPortGetCoreID()]){
            portYIELD_WITHIN_API();
        }
        else {
            /* Yield if the task was running on any other core */
            for(i = 0; i < portNUM_PROCESSORS; ++i){
               if(OS_current_TCB[i] == old_tcb){
                   vPortYieldOtherCore(i);
               }
            }
        }
    }
    portEXIT_CRITICAL(&OS_schedule_mutex);
    return OS_NO_ERROR;
}

/*******************************************************************************
* OS Schedule Delay Task
*
*   tcb = A pointer to the TCB of the task to delay. Use NULL to delay the
*         currently running TCB
*   tick_delay = The amount of ticks to delay the task for.
* 
* PURPOSE :
*
*   Delay the task the amount of ticks specified. Just forces a reschedule if the
*   delay value is zero
* 
* RETURN : 
*
*   Returns an error code or 0 (OS_NO_ERROR) if no error occured
*
* NOTES: 
*******************************************************************************/

int OS_schedule_delay_task(TCB_t *tcb, const TickType_t tick_delay)
{
    int i;
    TickType_t wakeup_time;

    /* Scheduler must be running in order to delay a task */
    if(OS_scheduler_running == OS_FALSE) {
        return OS_ERROR_SCHEDULER_STOPPED;
    }

    /* Tick delay must be greater than or equal to zero */
    if(tick_delay < 0){
        return OS_ERROR_INVALID_DLY;
    }

    portENTER_CRITICAL(&OS_schedule_mutex);
    wakeup_time = tick_delay + OS_tick_counter;
    
    if(tcb == NULL){
        tcb = OS_current_TCB[xPortGetCoreID()];
    }

    /* Remove ourselves from existing task list */
    switch(tcb->task_state) {
        case OS_TASK_STATE_RUNNING:
            _OS_ready_list_remove(tcb);
            break;
        case OS_TASK_STATE_READY:
            _OS_ready_list_remove(tcb);
            break;
        case OS_TASK_STATE_DELAYED:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_DELAYED_TASK;
        case OS_TASK_STATE_SUSPENDED:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_SUSPENDED_TASK;
        case OS_TASK_STATE_PENDING_DELETION:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_DELETED_TASK;
        case OS_TASK_STATE_READY_TO_DELETE:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_DELETED_TASK;
        default:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_INVALID_TSK_STATE;
    }

    /* Set the state of the current TCB to delayed */
    tcb->task_state = OS_TASK_STATE_DELAYED;

    /* Add to a list of delayed tasks */
    tcb->delay_wakeup_time = wakeup_time;
    _OS_delayed_list_insert(tcb);

    portEXIT_CRITICAL(&OS_schedule_mutex);
    
    /* Yield if we suspended the TCB on this core */
    if(tcb == OS_current_TCB[xPortGetCoreID()]) {
        if(OS_scheduler_running == OS_TRUE){
            portYIELD_WITHIN_API();
        }
        else {
            OS_yield_pending_list[xPortGetCoreID()] = OS_TRUE;
        }
    }

    /* Yield if we suspended the TCB on another core */
    else {
        for(i = 0; i < portNUM_PROCESSORS; ++i){
            if(tcb == OS_current_TCB[i]) {
                if(OS_scheduler_running == OS_TRUE){
                    _OS_schedule_yield_other_core(i, tcb->priority); 
                }
                else {
                    OS_yield_pending_list[i] = OS_TRUE;
                }
                break;
            }
        }
    }
    return OS_NO_ERROR;
}

/*******************************************************************************
* OS Schedule Suspend Task
*
*   tcb = A pointer to the tcb to be suspended. Use NULL to suspend the 
*         currently running TCB
* 
* PURPOSE : 
*
*   Suspend a task for an unspecified amount of time. Used by blocking APIs
*
* RETURN : 
*
*   Return an error code or 0 (OS_NO_ERROR) if no error occured
*
* NOTES: 
*
*******************************************************************************/

/* TODO: This is its own API for now but one day I may merge with OS_schedule_delay_task */
int OS_schedule_suspend_task(TCB_t *tcb)
{
    int i;

    /* Scheduler must be running in order to delay a task */
    if(OS_scheduler_running == OS_FALSE) {
        return OS_ERROR_SCHEDULER_STOPPED;
    }

    portENTER_CRITICAL(&OS_schedule_mutex);
    if(tcb == NULL) {
        tcb = OS_current_TCB[xPortGetCoreID()];
    }

    /* We need to remove the task list off of either the list it is currently on */
    switch(tcb->task_state) {
        case OS_TASK_STATE_RUNNING:
            _OS_ready_list_remove(tcb);
            break;
        case OS_TASK_STATE_READY:
            _OS_ready_list_remove(tcb);
            break;
        case OS_TASK_STATE_DELAYED:
            _OS_delayed_list_remove(tcb);
            break;
        case OS_TASK_STATE_SUSPENDED:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_NO_ERROR;
        case OS_TASK_STATE_PENDING_DELETION:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_DELETED_TASK;
        case OS_TASK_STATE_READY_TO_DELETE:
            return OS_ERROR_DELETED_TASK;
        default:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_INVALID_TSK_STATE;
    }
    tcb->task_state = OS_TASK_STATE_SUSPENDED;

    /* This line is only necessary if still reliant on FreeRTOS API */
    if( listLIST_ITEM_CONTAINER( &( tcb->xEventListItem ) ) != NULL )
	{
		( void ) uxListRemove( &( tcb->xEventListItem ) );
	}

    _OS_suspended_list_insert(tcb);
    portEXIT_CRITICAL(&OS_schedule_mutex);

    /* Yield if we suspended the TCB on this core */
    if(tcb == OS_current_TCB[xPortGetCoreID()]) {
        if(OS_scheduler_running == OS_TRUE){
            portYIELD_WITHIN_API();
        }
        else {
            OS_yield_pending_list[xPortGetCoreID()] = OS_TRUE;
        }
    }

    /* Yield if we suspended the TCB on another core */
    else {
        for(i = 0; i < portNUM_PROCESSORS; ++i){
            if(tcb == OS_current_TCB[i]) {
                if(OS_scheduler_running == OS_TRUE){
                    _OS_schedule_yield_other_core(i, tcb->priority); 
                }
                else {
                    OS_yield_pending_list[i] = OS_TRUE;
                }
                break;
            }
        }
    }
    return OS_NO_ERROR;
}

/*******************************************************************************
* OS Schedule Resume Task
*
*   tcb = A pointer to the tcb that has been suspended or delayed
* 
* PURPOSE :
*
*   Resume a task that has either been suspended or delayed. Add it back into
*   scheduling rotation 
*
* RETURN : 
*
*   Return an error code or 0 (OS_NO_ERROR) if no error occured
*
* NOTES: 
*
*******************************************************************************/

int OS_schedule_resume_task(TCB_t *tcb)
{
    int i;

    if(OS_scheduler_running == OS_FALSE) {
        return OS_ERROR_SCHEDULER_STOPPED;
    }

    portENTER_CRITICAL(&OS_schedule_mutex);

    /* We need to remove the task list off of either the delayed or suspended list */
    switch(tcb->task_state) {
        case OS_TASK_STATE_RUNNING:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_RUNNING_TASK;
            break;
        case OS_TASK_STATE_READY:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_READY_TASK;
            break;
        case OS_TASK_STATE_DELAYED:
            _OS_delayed_list_remove(tcb);
            break;
        case OS_TASK_STATE_SUSPENDED:
            _OS_suspended_list_remove(tcb);
            break;
        case OS_TASK_STATE_PENDING_DELETION:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_DELETED_TASK;
        case OS_TASK_STATE_READY_TO_DELETE:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_DELETED_TASK;
        default:
            assert(OS_FALSE);
    }

    /* Make the resumed task ready */
    tcb->task_state = OS_TASK_STATE_READY;
    _OS_ready_list_insert(tcb);

    portEXIT_CRITICAL(&OS_schedule_mutex);
    
    /* Run the resumed task if its priority is greater than any running task */
    if(tcb->priority > OS_current_TCB[xPortGetCoreID()]->priority) {
        portYIELD_WITHIN_API();
    }
    else {
        for(i = 0; i < portNUM_PROCESSORS; ++i){
           if(tcb->priority > OS_current_TCB[i]->priority) {
               _OS_schedule_yield_other_core(i, tcb->priority); 
               break;
           }
        }
    }
    return OS_NO_ERROR;
}

/*******************************************************************************
* OS Schedule Process Tick
* 
* PURPOSE : 
*
*   Notifies the kernel a tick has occured. Updates delay counters and more.
* 
* RETURN :
*
*   Returns a boolean stating whether or not a context switch is required.
*
* NOTES: 
*
*   Portable/hardware layer is expected to call OS_schedule_switch_context
*   if this function returns true.
*******************************************************************************/

OSBool_t OS_schedule_process_tick(void)
{
    uint8_t context_switch_required = OS_FALSE;

    /* Make sure we yield at the end if a yield is pending */
    if(OS_yield_pending_list[xPortGetCoreID()] == OS_TRUE) {
        context_switch_required = OS_TRUE;
    }

    /* See if the current core is in an ISR context */
    if (xPortInIsrContext()) {
        vApplicationTickHook();

        /* Only core 0 can increment the tick count */
        if (xPortGetCoreID() != 0 ) {
			return OS_TRUE;
		}
    }

    /* Any core can unwind pending ticks while resuming all tasks */
    if( OS_suspended_schedulers_list[ xPortGetCoreID() ] != OS_FALSE) {
        ++OS_pending_ticks;
        return context_switch_required;
    }

    portENTER_CRITICAL_ISR(&OS_schedule_mutex);

    OS_tick_counter++;
    if(OS_tick_counter == (TickType_t)0){
        assert(OS_FALSE); /* TODO */
        ++OS_tick_overflow_counter;
    }

    /* Wakeup any tasks whose timers have expired */
    while((OS_delayed_list.head_ptr != NULL) && 
          (OS_delayed_list.head_ptr->delay_wakeup_time <= OS_tick_counter)) {

        /* Remove the task from an event list if it is on one */
		if( listLIST_ITEM_CONTAINER( &(OS_delayed_list.head_ptr->xEventListItem ) ) != NULL ){
			( void ) uxListRemove( &(OS_delayed_list.head_ptr->xEventListItem ) );
            context_switch_required = OS_TRUE;
        }

        /* Remove the task from a waiting list if it is on one */
        if(OS_delayed_list.head_ptr->waitlist != NULL){
            _OS_waitlist_remove(OS_delayed_list.head_ptr);
        }

        /* Wake up the task */
        if(_OS_delayed_list_wakeup_next_task() == OS_TRUE) {
            context_switch_required = OS_TRUE;
        }
    }

    /* If our current task is in a round robin list, we will have to switch */
    if(OS_ready_list[OS_current_TCB[xPortGetCoreID()]->priority].num_tasks > 1){
        context_switch_required = OS_TRUE;
    }
    
    portEXIT_CRITICAL_ISR(&OS_schedule_mutex);
    return context_switch_required;
}

/*******************************************************************************
* OS Schedule Change Task Priority
*
*   tcb = A pointer to the tcb that will undergo a priority change
*   new_prio = The new priority for the given task
* 
* PURPOSE : 
*
*   Change the priority of a task and force a reschedule if necessary
* 
* RETURN :
*
*   Return an error code or 0 (OS_NO_ERROR) if no error occured 
*
* NOTES: 
*******************************************************************************/

int OS_schedule_change_task_prio(TCB_t *tcb, TaskPrio_t new_prio)
{
    OSBool_t context_switch_required = OS_FALSE;
    TaskPrio_t old_prio;

    if(new_prio >= OS_MAX_PRIORITIES || new_prio <= 0) {
        return OS_ERROR_INVALID_PRIO;
    }
    
    portENTER_CRITICAL(&OS_schedule_mutex);
    
    /* If the tcb is null, get the current tcb */
    if(tcb == NULL){
        tcb = OS_schedule_get_current_tcb();
    }
    old_prio = tcb->priority;

    /* Nothing to do if the base priority equals the new priority */
    if(tcb->base_priority == new_prio){
        portEXIT_CRITICAL(&OS_schedule_mutex);
        return OS_NO_ERROR;
    }

    /* Task must be removed from any list prior to a priority change */
    switch(tcb->task_state) {
        case OS_TASK_STATE_RUNNING:
            _OS_ready_list_remove(tcb);
            break;
        case OS_TASK_STATE_READY:
            _OS_ready_list_remove(tcb);
            break;
        case OS_TASK_STATE_DELAYED:
            break;
        case OS_TASK_STATE_SUSPENDED:
            break;
        case OS_TASK_STATE_PENDING_DELETION:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_DELETED_TASK;
        case OS_TASK_STATE_READY_TO_DELETE:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_DELETED_TASK;
        default:
            portEXIT_CRITICAL(&OS_schedule_mutex);
            return OS_ERROR_INVALID_TSK_STATE;
    }
    
    if(tcb->base_priority == tcb->priority){
        tcb->priority = new_prio;
    }
    tcb->base_priority = new_prio;

    /* We can re-add to the ready list now that the priority is changed */
    if(tcb->task_state == OS_TASK_STATE_READY || tcb->task_state == OS_TASK_STATE_RUNNING) {
        _OS_ready_list_insert(tcb);
    }

    if( ( listGET_LIST_ITEM_VALUE( &( tcb->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL ){
		listSET_LIST_ITEM_VALUE( &( tcb->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t )new_prio) );
	}

    /* We raised a task's priority but its not the current task */
    if(new_prio > old_prio && tcb != OS_current_TCB[xPortGetCoreID()]) {
        /* Should run on this core right now */
        if(tcb->core_ID == xPortGetCoreID() && new_prio >= OS_current_TCB[xPortGetCoreID()]->priority) {
            context_switch_required = OS_TRUE;
        }
        /* Might run on the other core right now */
        else if(tcb->core_ID != xPortGetCoreID()){
            context_switch_required = OS_TRUE;
        }        
    }

    /* Reducing the priority of the current task */
    if(old_prio > new_prio && tcb == OS_current_TCB[xPortGetCoreID()]) {
        context_switch_required = OS_TRUE;
    }

    /* If a context switch is required, yield on the right core */
    if(context_switch_required == OS_TRUE){
        if(tcb->core_ID != xPortGetCoreID()){
            _OS_schedule_yield_other_core(tcb->core_ID, new_prio);
        }
        else {
            portYIELD_WITHIN_API();
        }
    }

    portEXIT_CRITICAL(&OS_schedule_mutex);
    return OS_NO_ERROR;
}

/*******************************************************************************
* OS Schedule Raise Priority Mutex Holder
*
*   mutex_holder = A pointer to the task that is inheriting a priority
* 
* PURPOSE : 
*
*   This is the API for priority inheritance to solve problems of priority
*   inversion. 
* 
* RETURN : 
*
* NOTES: 
*******************************************************************************/

void OS_schedule_raise_priority_mutex_holder(TCB_t *mutex_holder)
{
    portENTER_CRITICAL(&OS_schedule_mutex);

    if(mutex_holder == NULL || 
            (mutex_holder->priority >= OS_current_TCB[xPortGetCoreID()]->priority)) {
        portEXIT_CRITICAL(&OS_schedule_mutex);
        return;
    }
    portENTER_CRITICAL(&OS_schedule_mutex);

    /* Adjust the mutex holder state to account for its new
	priority.  Only reset the event list item value if the value is
	not	being used for anything else. */
	if( ( listGET_LIST_ITEM_VALUE( &( mutex_holder->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL ){
		listSET_LIST_ITEM_VALUE( &( mutex_holder->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) OS_current_TCB[ xPortGetCoreID() ]->priority);
	}

    if(mutex_holder->task_state == OS_TASK_STATE_READY || mutex_holder->task_state == OS_TASK_STATE_RUNNING) {
        _OS_ready_list_remove(mutex_holder);
        mutex_holder->priority = OS_current_TCB[xPortGetCoreID()]->priority;
        _OS_ready_list_insert(mutex_holder);
    }
    else {
       mutex_holder->priority = OS_current_TCB[xPortGetCoreID()]->priority; 
    }
    portEXIT_CRITICAL(&OS_schedule_mutex);

    portEXIT_CRITICAL(&OS_schedule_mutex);
    return;
}

/*******************************************************************************
* OS Schedule Revert Priority Mutex Holder
*
*   mux_holder = A pointer to the task that previously inherited a priority
* 
* PURPOSE : 
*
*   After releasing a mutex in a priority inheritance context, this function is 
*   used to return the task back to its previous priority.
* 
* RETURN : 
*
* NOTES:
*******************************************************************************/

OSBool_t OS_schedule_revert_priority_mutex_holder(void * const mux_holder)
{
    TCB_t * const mutex_holder = (TCB_t *)mux_holder;
    OSBool_t ret_val = OS_FALSE;

    portENTER_CRITICAL(&OS_schedule_mutex);

    if(mutex_holder == NULL){
        portEXIT_CRITICAL(&OS_schedule_mutex);
        return ret_val;
    }
    mutex_holder->mutexes_held--;

    if(mutex_holder->priority != mutex_holder->base_priority){
		/* Only disinherit if no other mutexes are held. */
		if(mutex_holder->mutexes_held == 0) {
			portENTER_CRITICAL(&OS_schedule_mutex);

            if(mutex_holder->task_state == OS_TASK_STATE_READY || 
                    mutex_holder->task_state == OS_TASK_STATE_RUNNING) {
                _OS_ready_list_remove(mutex_holder);
                mutex_holder->priority = mutex_holder->base_priority;
                _OS_ready_list_insert(mutex_holder);
            }
            else {
                mutex_holder->priority = mutex_holder->base_priority;
            }

			/* Reset the event list item value.  It cannot be in use for
			any other purpose if this task is running, and it must be
			running to give back the mutex. */
            listSET_LIST_ITEM_VALUE( &( mutex_holder->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) mutex_holder->priority ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

			ret_val = OS_TRUE;
			portEXIT_CRITICAL(&OS_schedule_mutex);
		}
	}
    portEXIT_CRITICAL(&OS_schedule_mutex);
    return ret_val;
}

/*******************************************************************************
* OS Schedule Get Idle TCB
*
*   core_ID = The core for which we want to retrieve the idle TCB pointer 
* 
* PURPOSE : 
*
*   Get a pointer to the idle task associated with a given core
* 
* RETURN : 
*
*   The pointer to the idle task associated with a given core
*
* NOTES: 
*******************************************************************************/

TCB_t* OS_schedule_get_idle_tcb(int core_ID)
{
    TCB_t *idle = OS_idle_tcb_list[core_ID];
    return idle;
}

/*******************************************************************************
* OS Schedule Get Current TCB
* 
* PURPOSE : 
*
*   Get the TCB pointer associated with the core we are currently on
* 
* RETURN : 
*
*   The pointer to the task running on the current core
*
* NOTES: 
*******************************************************************************/

TCB_t* OS_schedule_get_current_tcb(void)
{
    TCB_t *current_tcb;

    unsigned state = portENTER_CRITICAL_NESTED();
    current_tcb = OS_current_TCB[xPortGetCoreID()];
    portEXIT_CRITICAL_NESTED(state);

    return current_tcb;
}

/*******************************************************************************
* OS Schedule Get Current TCB From Core
*
*   core_ID = The core that contains the TCB we are looking for
* 
* PURPOSE : 
*
*   Get the TCB pointer associated with the core_ID given
* 
* RETURN : 
*
*   The pointer to the task runnning on the given core
*
* NOTES:
*******************************************************************************/

/* TODO Eventually merge this with the other? */
TCB_t* OS_schedule_get_current_tcb_from_core(int core_ID)
{
    TCB_t *current_tcb = NULL;

    unsigned state = portENTER_CRITICAL_NESTED();
    current_tcb = OS_current_TCB[core_ID];
    portEXIT_CRITICAL_NESTED(state);

    return current_tcb;
}

/*******************************************************************************
* OS Schedule Get State
* 
* PURPOSE : 
*
*   Get the state enumeration associated with the entire scheduler
* 
* RETURN : 
*
*   An enumerated scheduler state representing the schedulers current state
*
* NOTES: this function isn't failing
*******************************************************************************/

OSScheduleState_t OS_schedule_get_state(void)
{
    unsigned state;
    OSScheduleState_t current_state;
    
    state = portENTER_CRITICAL_NESTED();
    if (OS_scheduler_running == OS_FALSE) {
        current_state = OS_SCHEDULE_STATE_STOPPED;
    }
    else if (OS_suspended_schedulers_list[xPortGetCoreID()] != OS_FALSE) {
        current_state = OS_SCHEDULE_STATE_SUSPENDED;
    }
    else {
        current_state = OS_SCHEDULE_STATE_RUNNING;
    }
    portEXIT_CRITICAL_NESTED(state);
    return current_state;
}

/*******************************************************************************
* OS Schedule Get Tick Count
* 
* PURPOSE : 
*
*   Get the value of the tick counter
* 
* RETURN : 
*
*   Return the current value of our scheduler tick counter
*
* NOTES:
*
*   No writing is taking place so we allow this to exist without critical
*   secitons. The tick count will be outdated soon anyways...
*******************************************************************************/

TickType_t OS_schedule_get_tick_count(){
    return OS_tick_counter;
}

/*******************************************************************************
* OS __getreent
* 
* PURPOSE : 
*
*   Get the reentrant structure
* 
* RETURN : 
*
*   The reentrant structure
*
* NOTES:
*
*   User code need not worry about this odd bit of code. It just works
*******************************************************************************/

struct _reent* __getreent(void) {
	//No lock needed because if this changes, we won't be running anymore.
	TCB_t *currTask=OS_schedule_get_current_tcb();
	if (currTask==NULL) {
		//No task running. Return global struct.
		return _GLOBAL_REENT;
	} else {
		//We have a task; return its reentrant struct.
		return &currTask->xNewLib_reent;
	}
}

/*******************************************************************************
* This next group of functions were necessary for integration with the existing
* ESP-IDF codebase. They are based entirely off of equivalent FreeRTOS API calls.
* They are mostly necessary for the semaphore, mutex, timer, and queue systems.
* One day these systems will be replaced entirely and our OS will be free from
* The tyrany of FreeRTOS. But for today they will remain as is....
*******************************************************************************/

void * OS_schedule_increment_task_mutex_count(void)
{
    TCB_t *cur_tcb;
    portENTER_CRITICAL(&OS_schedule_mutex);
    if(OS_current_TCB[xPortGetCoreID()] != NULL) {
        OS_current_TCB[xPortGetCoreID()]->mutexes_held++;
    }
    cur_tcb = OS_current_TCB[xPortGetCoreID()];
    portEXIT_CRITICAL(&OS_schedule_mutex);
    return cur_tcb;
}

void OS_set_timeout_state( TimeOut_t * const pxTimeOut )
{
	assert( pxTimeOut );
	pxTimeOut->xOverflowCount = OS_tick_overflow_counter;
	pxTimeOut->xTimeOnEntering = OS_tick_counter;
}

OSBool_t OS_schedule_check_for_timeout( TimeOut_t * const timeout, TickType_t * const ticks_to_wait)
{
    OSBool_t ret_val;
    portENTER_CRITICAL(&OS_schedule_mutex);

    if(*ticks_to_wait == portMAX_DELAY) {
        ret_val = OS_FALSE;
    }
    if( ( OS_tick_overflow_counter != timeout->xOverflowCount ) && 
        ( OS_tick_counter >= timeout->xTimeOnEntering ) ){
		ret_val = OS_TRUE;
	}
	else if( ( OS_tick_counter - timeout->xTimeOnEntering ) < *ticks_to_wait) {
		*ticks_to_wait -= ( OS_tick_counter - timeout->xTimeOnEntering );
		OS_set_timeout_state( timeout );
        ret_val = OS_FALSE;
	}
	else{
		ret_val = OS_TRUE;
	}

    portEXIT_CRITICAL(&OS_schedule_mutex);
    return ret_val;
}

void _OS_schedule_yield_other_core(int core_ID, TaskPrio_t prio )
{
	TCB_t *cur_tcb = OS_current_TCB[core_ID];
	int i;

	if (core_ID != CORE_NO_AFFINITY) {
		if ( cur_tcb->priority < prio ) {
            assert(core_ID < portNUM_PROCESSORS);
			vPortYieldOtherCore( core_ID );
		}
	}
	else
	{
		/* The task has no affinity. See if we can find a CPU to put it on.*/
		for (i=0; i< portNUM_PROCESSORS; i++) {
			if (i != xPortGetCoreID() && OS_current_TCB[i]->priority < prio)
			{
				vPortYieldOtherCore( i );
				break;
			}
		}
	}
}

/* TODO: This will eventually be depricated and replaced with OS_schedule_suspend_task */
void OS_schedule_place_task_on_event_list(List_t * const pxEventList, const TickType_t ticks_to_wait)
{
    TCB_t *cur_tcb;
    TickType_t wakeup_time;
    assert( pxEventList );

    portENTER_CRITICAL(&OS_schedule_mutex);

    cur_tcb = OS_current_TCB[xPortGetCoreID()];

    vListInsert( pxEventList, &(cur_tcb->xEventListItem ) );
    /* I (right now) believe it will be necessary to assume the task is ready */
    if(cur_tcb->task_state == OS_TASK_STATE_READY || cur_tcb->task_state == OS_TASK_STATE_RUNNING){
        _OS_ready_list_remove(cur_tcb);
    }
    else {
        assert(OS_FALSE);
    }

    if(ticks_to_wait == portMAX_DELAY) {
        _OS_suspended_list_insert(cur_tcb);
        cur_tcb->task_state = OS_TASK_STATE_SUSPENDED;
    }
    else {
        wakeup_time = OS_tick_counter + ticks_to_wait;
        cur_tcb->delay_wakeup_time = wakeup_time;
        _OS_delayed_list_insert(cur_tcb);
        cur_tcb->task_state = OS_TASK_STATE_DELAYED;
    }

    portEXIT_CRITICAL(&OS_schedule_mutex);
}

void OS_schedule_place_task_on_events_list_restricted(List_t * const pxEventList, const TickType_t ticks_to_wait)
{
    TickType_t wakeup_time; 
    TCB_t *cur_tcb;

    portENTER_CRITICAL(&OS_schedule_mutex);

    cur_tcb = OS_current_TCB[xPortGetCoreID()];

    vListInsertEnd( pxEventList, &( cur_tcb->xEventListItem ) );
    
    /* TODO: THIS and the one above will become a switch case */
    if(cur_tcb->task_state == OS_TASK_STATE_READY || cur_tcb->task_state == OS_TASK_STATE_RUNNING){
        _OS_ready_list_remove(cur_tcb);
    }
    else {
        assert(OS_FALSE);
    }

	wakeup_time = OS_tick_counter + ticks_to_wait;

    cur_tcb->delay_wakeup_time = wakeup_time;
    _OS_delayed_list_insert(cur_tcb);
    cur_tcb->task_state = OS_TASK_STATE_DELAYED;

	portEXIT_CRITICAL(&OS_schedule_mutex);
}

void OS_schedule_place_task_on_unordered_events_list(List_t *pxEventList, const TickType_t item_value, const TickType_t ticks_to_wait)
{
    TickType_t wakeup_time;
    TCB_t * cur_tcb;

    assert(pxEventList);

    portENTER_CRITICAL(&OS_schedule_mutex);

    cur_tcb = OS_current_TCB[xPortGetCoreID()];

    listSET_LIST_ITEM_VALUE( &( cur_tcb->xEventListItem ), item_value | taskEVENT_LIST_ITEM_VALUE_IN_USE );
    vListInsertEnd( pxEventList, &( cur_tcb->xEventListItem ) );
    
    if(cur_tcb->task_state == OS_TASK_STATE_READY || cur_tcb->task_state == OS_TASK_STATE_RUNNING){
        _OS_ready_list_remove(cur_tcb);
    }
    else {
        assert(OS_FALSE);
    }

    if(ticks_to_wait == portMAX_DELAY) {
        _OS_suspended_list_insert(cur_tcb);
        cur_tcb->task_state = OS_TASK_STATE_SUSPENDED;
    }
    else {
        wakeup_time = OS_tick_counter + ticks_to_wait;
        cur_tcb->delay_wakeup_time = wakeup_time;
        _OS_delayed_list_insert(cur_tcb);
        cur_tcb->task_state = OS_TASK_STATE_DELAYED;
    }

    portEXIT_CRITICAL(&OS_schedule_mutex);
}

int OS_schedule_remove_task_from_event_list(const List_t * const pxEventList)
{
    TCB_t *unblocked_tcb = NULL;
    int ret_val;
    OSBool_t task_can_be_ready;
    int i, target_cpu;

    portENTER_CRITICAL_ISR(&OS_schedule_mutex);
	/* The event list is sorted in priority order, so the first in the list can
	be removed as it is known to be the highest priority.  Remove the TCB from
	the delayed list, and add it to the ready list.

	If an event is for a queue that is locked then this function will never
	get called - the lock count on the queue will get modified instead.  This
	means exclusive access to the event list is guaranteed here.

	This function assumes that a check has already been made to ensure that
	pxEventList is not empty. */
	if ( ( listLIST_IS_EMPTY( pxEventList ) ) == pdFALSE ) {
	    unblocked_tcb = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxEventList );
		assert( unblocked_tcb );
		( void ) uxListRemove( &( unblocked_tcb->xEventListItem ) );
	} else {
		portEXIT_CRITICAL_ISR(&OS_schedule_mutex);
		return pdFALSE;
	}

    /* Determine if the task can possibly be run on either CPU now, either because the scheduler
	   the task is pinned to is running or because a scheduler is running on any CPU. */
	task_can_be_ready = OS_FALSE;
	if ( unblocked_tcb->core_ID == CORE_NO_AFFINITY ) {
		target_cpu = xPortGetCoreID();
		for (i = 0; i < portNUM_PROCESSORS; i++) {
			if ( OS_suspended_schedulers_list[ i ] == OS_FALSE) {
				task_can_be_ready = OS_TRUE;
				break;
			}
		}
	} else {
		target_cpu = unblocked_tcb->core_ID;
		task_can_be_ready = OS_suspended_schedulers_list[target_cpu] == OS_FALSE;
	}

    switch(unblocked_tcb->task_state){
        case OS_TASK_STATE_RUNNING:
            assert(OS_FALSE);
            break;
        case OS_TASK_STATE_READY:
            assert(0 == 1);
            break;
        case OS_TASK_STATE_DELAYED:
            _OS_delayed_list_remove(unblocked_tcb);
            break;
        case OS_TASK_STATE_SUSPENDED:
            _OS_suspended_list_remove(unblocked_tcb);
            break;
        case OS_TASK_STATE_PENDING_DELETION:
            assert(1 == 0);
            break;
        case OS_TASK_STATE_READY_TO_DELETE:
            assert(1 == 0);
            break;
        default:
            assert(1 == 0); 
    }

    if( task_can_be_ready == OS_TRUE )
	{
        _OS_ready_list_insert(unblocked_tcb);
        unblocked_tcb->task_state = OS_TASK_STATE_READY;
	}
	else
	{
		/* The delayed and ready lists cannot be accessed, so hold this task
		pending until the scheduler is resumed on this CPU. */
        _OS_pending_ready_list_insert(unblocked_tcb, target_cpu);
        unblocked_tcb->task_state = OS_TASK_STATE_PENDING_READY;
	}

	if ( (unblocked_tcb->core_ID == xPortGetCoreID() || unblocked_tcb->core_ID == CORE_NO_AFFINITY) && unblocked_tcb->priority >= OS_current_TCB[ xPortGetCoreID() ]->priority )
	{
		/* Return true if the task removed from the event list has a higher
		priority than the calling task.  This allows the calling task to know if
		it should force a context switch now. */
		ret_val = pdTRUE;

		/* Mark that a yield is pending in case the user is not using the
		"xHigherPriorityTaskWoken" parameter to an ISR safe FreeRTOS function. */
		OS_yield_pending_list[ xPortGetCoreID() ] = OS_TRUE;
	}
	else if ( unblocked_tcb->core_ID != xPortGetCoreID() )
	{
		_OS_schedule_yield_other_core(unblocked_tcb->core_ID, unblocked_tcb->priority);
		ret_val = pdFALSE;
	}
	else
	{
		ret_val = pdFALSE;
	}

	#if( configUSE_TICKLESS_IDLE == 1 )
	{
		/* If a task is blocked on a kernel object then xNextTaskUnblockTime
		might be set to the blocked task's time out time.  If the task is
		unblocked for a reason other than a timeout xNextTaskUnblockTime is
		normally left unchanged, because it is automatically get reset to a new
		value when the tick count equals xNextTaskUnblockTime.  However if
		tickless idling is used it might be more important to enter sleep mode
		at the earliest possible time - so reset xNextTaskUnblockTime here to
		ensure it is updated at the earliest possible time. */
        _OS_update_next_task_unblock_time();
	}
	#endif
	portEXIT_CRITICAL_ISR(&OS_schedule_mutex);

	return ret_val;
}

int OS_schedule_remove_task_from_unordered_events_list(ListItem_t * pxEventListItem, const TickType_t item_value)
{
    TCB_t *unblocked_tcb;
    int ret_val;

    portENTER_CRITICAL(&OS_schedule_mutex);
    assert(OS_suspended_schedulers_list[xPortGetCoreID()] != OS_FALSE);

    listSET_LIST_ITEM_VALUE( pxEventListItem, item_value | taskEVENT_LIST_ITEM_VALUE_IN_USE );

    unblocked_tcb = ( TCB_t * ) listGET_LIST_ITEM_OWNER( pxEventListItem );
	assert(unblocked_tcb);
	( void ) uxListRemove( pxEventListItem );

    switch(unblocked_tcb->task_state){
        case OS_TASK_STATE_RUNNING:
            assert(OS_FALSE);
            break;
        case OS_TASK_STATE_READY:
            assert(OS_FALSE);
            break;
        case OS_TASK_STATE_DELAYED:
            _OS_delayed_list_remove(unblocked_tcb);
            break;
        case OS_TASK_STATE_SUSPENDED:
            _OS_suspended_list_remove(unblocked_tcb);
            break;
        case OS_TASK_STATE_PENDING_DELETION:
            assert(OS_FALSE);
            break;
        case OS_TASK_STATE_READY_TO_DELETE:
            assert(OS_FALSE);
            break;
        default:
            assert(OS_FALSE); 
    }
    unblocked_tcb->task_state = OS_TASK_STATE_READY;
    _OS_ready_list_insert(unblocked_tcb);

    if ( (unblocked_tcb->core_ID == xPortGetCoreID() || unblocked_tcb->core_ID == CORE_NO_AFFINITY) && unblocked_tcb->priority >= OS_current_TCB[ xPortGetCoreID() ]->priority )
	{
		/* Return true if the task removed from the event list has
		a higher priority than the calling task.  This allows
		the calling task to know if it should force a context
		switch now. */
		ret_val = pdTRUE;

		/* Mark that a yield is pending in case the user is not using the
		"xHigherPriorityTaskWoken" parameter to an ISR safe FreeRTOS function. */
		OS_yield_pending_list[ xPortGetCoreID() ] = OS_TRUE;
	}
	else if ( unblocked_tcb->core_ID != xPortGetCoreID() )
	{
		_OS_schedule_yield_other_core( unblocked_tcb->core_ID, unblocked_tcb->priority );
		ret_val = pdFALSE;
	}
	else
	{
		ret_val = pdFALSE;
	}

	portEXIT_CRITICAL(&OS_schedule_mutex);
	return ret_val;
}

TickType_t OS_schedule_reset_task_event_item_value(void)
{
    TickType_t ret_val;
    portENTER_CRITICAL(&OS_schedule_mutex);
    ret_val = listGET_LIST_ITEM_VALUE( &( OS_current_TCB[ xPortGetCoreID() ]->xEventListItem ) );

	/* Reset the event list item to its normal value - so it can be used with
	queues and semaphores. */
	listSET_LIST_ITEM_VALUE( &( OS_current_TCB[ xPortGetCoreID() ]->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) OS_current_TCB[ xPortGetCoreID() ]->priority ) ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
	portEXIT_CRITICAL(&OS_schedule_mutex);

	return ret_val;
}

/*******************************************************************************
* STATIC FUNCTION DEFINITIONS
*******************************************************************************/

/**
 * This updates the global variable keeping track of the next timeout.
 * Should get called in only three places:
 *  - When we cycle the overflow list
 *  - When we add a task to a delayed list
 *  - When we remove a task from the delayed list
 */
static void _OS_update_next_task_unblock_time(void)
{
    /* If the list of delayed tasks is empty */
    if(OS_delayed_list.head_ptr == NULL){
        OS_next_task_unblock_time = portMAX_DELAY;
        return;
    }
    OS_next_task_unblock_time = OS_delayed_list.head_ptr->delay_wakeup_time;
}

/**
 * Zeros out the bit map storing priorities that are in use
 */
static void _OS_bitmap_reset_prios(void)
{
    int i;
    for(i = 0; i < OS_PRIO_MAP_SIZE; ++i){
        OS_ready_priorities_map[i] = (uint8_t)0;
    }
}

/**
 * Return the highest priority that is currently assigned to any task
 */
static int _OS_schedule_get_highest_prio(void)
{
    uint8_t val;
    int leading_zeros = 0;
    int map_index = -1;

    while(OS_ready_priorities_map[++map_index] == (uint8_t)0 && map_index < OS_PRIO_MAP_SIZE);

    if(map_index == OS_PRIO_MAP_SIZE){
        /* No priorities in use in the map */
        return 0;
    }

    /* Count the leading zeros in the bit map index that contains a priority */
    val = OS_ready_priorities_map[map_index];
    while(!(val & (1 << 7))){
        val = (val << 1);
        ++leading_zeros;
    }

    return((OS_MAX_PRIORITIES) - ((map_index * 8) + leading_zeros));
}

/**
 * Add an entry to the priority bitmap corresponding to the new priority
 * Does nothing if the bit is already set to 1
 */
static void _OS_bitmap_add_prio(TaskPrio_t new_prio)
{
    TaskPrio_t index = (TaskPrio_t)((OS_MAX_PRIORITIES - new_prio) / 8);
    TaskPrio_t shift = (OS_MAX_PRIORITIES - new_prio) % 8;

    OS_ready_priorities_map[index] = OS_ready_priorities_map[index] | ((TaskPrio_t)128 >> shift);
}

/**
 * Remove the entry in the bitmap corresponding to the given priority
 * Should only be done if no tasks use that priority anymore
 */
static void _OS_bitmap_remove_prio(TaskPrio_t prio)
{
    TaskPrio_t index = (TaskPrio_t)((OS_MAX_PRIORITIES - prio) / 8);
    TaskPrio_t shift = (OS_MAX_PRIORITIES - prio) % 8;

    OS_ready_priorities_map[index] = OS_ready_priorities_map[index] ^ ((TaskPrio_t)128 >> shift);
}

/**
 * Ensures that all entries in the ready list are reset
 */
static void _OS_ready_list_init(void)
{
    int index;
    for(index = 0; index < OS_MAX_PRIORITIES; ++index){
        OS_ready_list[index].num_tasks = 0;
        OS_ready_list[index].head_ptr = NULL;
        OS_ready_list[index].tail_ptr = NULL;
    }
}

/**
 * Responsible for placing the tcb in the ready list
 * Application code should not call! This is a helper for OS_add_task_to_ready_list
 */
static void _OS_ready_list_insert(TCB_t *new_tcb)
{
    int prio = new_tcb->priority;
    if(OS_ready_list[prio].num_tasks == 0){
        _OS_bitmap_add_prio(prio);
    }
    _OS_task_list_append(new_tcb, &(OS_ready_list[prio]));
}

/**
 * Removes a tcb from the ready list
 */
static void _OS_ready_list_remove(TCB_t *tcb)
{
    assert(tcb->task_state == OS_TASK_STATE_READY ||
            tcb->task_state == OS_TASK_STATE_RUNNING);
    int prio = tcb->priority;
    if(OS_ready_list[prio].num_tasks == 1) {
        _OS_bitmap_remove_prio(prio);
    }
    _OS_task_list_remove(tcb, &(OS_ready_list[prio]));
}

/**
 * Inserts a task into the deletion pending list
 */
static void _OS_deletion_pending_list_insert(TCB_t *tcb)
{
    _OS_task_list_append(tcb, &(OS_deletion_pending_list));
}

/**
 * Empties out the deletion pending list and should destroy all the tasks
 */
static void _OS_deletion_pending_list_empty()
{
    TCB_t * tcb = OS_deletion_pending_list.head_ptr;
    while(OS_deletion_pending_list.num_tasks > 0){
        assert(OS_FALSE);
        /* TODO */
    }
}

/**
 * Insert a task into the delayed list
 */
static void _OS_delayed_list_insert(TCB_t *tcb)
{
    assert(tcb->next_ptr == NULL);
    assert(tcb->prev_ptr == NULL);

    TCB_t *tcb1;
    TCB_t *tcb2;

    ++OS_num_tasks_delayed;

    /* Update the task counter for this delayed list */
    OS_delayed_list.num_tasks++;

    /* First entry in the delayed list */
    if(OS_delayed_list.head_ptr == NULL) {
        OS_delayed_list.head_ptr = tcb;
        OS_delayed_list.tail_ptr = tcb;
        _OS_update_next_task_unblock_time();
        return;
    }
    /* This task is the first one to get woken up */
    if(OS_delayed_list.head_ptr->delay_wakeup_time >= tcb->delay_wakeup_time) {
        tcb->prev_ptr = NULL;
        tcb->next_ptr = OS_delayed_list.head_ptr;
        OS_delayed_list.head_ptr->prev_ptr = tcb;
        OS_delayed_list.head_ptr = tcb;
        _OS_update_next_task_unblock_time();
        return;
    }
    /* This task is the last one to get woken up */
    if(OS_delayed_list.tail_ptr->delay_wakeup_time <= tcb->delay_wakeup_time) {
        tcb->next_ptr = NULL;
        tcb->prev_ptr = OS_delayed_list.tail_ptr;
        OS_delayed_list.tail_ptr->next_ptr = tcb;
        OS_delayed_list.tail_ptr = tcb;
        return;
    }
    /* Else, find the spot to place this task */
    tcb1 = OS_delayed_list.head_ptr;
    while(tcb1->delay_wakeup_time < tcb->delay_wakeup_time){
        tcb1 = tcb1->next_ptr;
    }
    tcb2 = tcb1->prev_ptr;

    tcb2->next_ptr = tcb;
    tcb1->prev_ptr = tcb;
    tcb->prev_ptr = tcb2;
    tcb->next_ptr = tcb1;
}

/**
 * Remove a task from the delayed list
 */
static void _OS_delayed_list_remove(TCB_t *tcb)
{
    assert(tcb->task_state == OS_TASK_STATE_DELAYED);
    OS_num_tasks_delayed--;
    if(tcb == OS_delayed_list.head_ptr) {
        _OS_update_next_task_unblock_time();
    }
    _OS_task_list_remove(tcb, &OS_delayed_list);
}

/**
 * Insert into the suspended tasks list
 */
static void _OS_suspended_list_insert(TCB_t *tcb)
{
    _OS_task_list_append(tcb, &OS_suspended_list);
}

/**
 * Remove from the suspended tasks list
 */
static void _OS_suspended_list_remove(TCB_t *tcb)
{
    assert(tcb->task_state == OS_TASK_STATE_SUSPENDED);
    _OS_task_list_remove(tcb, &OS_suspended_list);
}

/**
 * Wake up the next task on the delayed list.
 * This only deals with the main delayed list, we won't ever wakeup from the overflow list.
 * 
 * Returns True if a context switch is required after the wakeup. False otherwise.
 */
static OSBool_t _OS_delayed_list_wakeup_next_task(void)
{    
    OSBool_t context_switch_required = OS_FALSE;
    TCB_t *woken_task = OS_delayed_list.head_ptr;
    assert(woken_task != NULL);

    --OS_num_tasks_delayed;

    /* If this is the only entry on the delayed list */
    if(OS_delayed_list.num_tasks == 1){
        OS_delayed_list.head_ptr = NULL;
        OS_delayed_list.tail_ptr = NULL;
        OS_delayed_list.num_tasks = 0;
    } 
    else {
        OS_delayed_list.head_ptr = woken_task->next_ptr;
        assert(OS_delayed_list.head_ptr);
        OS_delayed_list.head_ptr->prev_ptr = NULL;
        OS_delayed_list.num_tasks--;
    }

    /* Does the woken task have a higher priority than the running task? */
    if(woken_task->priority >= OS_current_TCB[xPortGetCoreID()]->priority){
        context_switch_required = OS_TRUE;
    }

    /* place task on the ready list */
    woken_task->next_ptr = NULL;
    woken_task->prev_ptr = NULL;
    woken_task->task_state = OS_TASK_STATE_READY;
    _OS_ready_list_insert(woken_task);

    /* Update the time of the next wakeup to occur in the delayed list */
    _OS_update_next_task_unblock_time();

    return context_switch_required;
}

/**
 * Removes from the head of the pending ready list.
 * Adds that task to the ready list
 */
static void _OS_pending_ready_list_schedule_next_task(void)
{
    int core_ID = xPortGetCoreID();
    TCB_t *scheduled_tcb = OS_pending_ready_list[core_ID].head_ptr;
    assert(scheduled_tcb != NULL);

    /* If this is the only entry on the pending ready list */
    if(OS_pending_ready_list[core_ID].num_tasks == 1){
        OS_pending_ready_list[core_ID].head_ptr = NULL;
        OS_pending_ready_list[core_ID].tail_ptr = NULL;
        OS_pending_ready_list[core_ID].num_tasks = 0;        
    }
    else {
        OS_pending_ready_list[core_ID].head_ptr = scheduled_tcb->next_ptr;
        OS_pending_ready_list[core_ID].head_ptr->prev_ptr = NULL;
        OS_pending_ready_list[core_ID].num_tasks--;
    }
	//( void ) uxListRemove( &( scheduled_tcb->xEventListItem ) );


    /* Place on the ready list */
    scheduled_tcb->next_ptr = NULL;
    scheduled_tcb->prev_ptr = NULL;
    scheduled_tcb->task_state = OS_TASK_STATE_READY;
    _OS_ready_list_insert(scheduled_tcb);

    /* TODO First part of if statement should be unnecessary. Remove */
    if((scheduled_tcb->core_ID == xPortGetCoreID() || scheduled_tcb->core_ID == CORE_NO_AFFINITY) && (scheduled_tcb->priority >= OS_current_TCB[core_ID]->priority)) {
        OS_yield_pending_list[core_ID] = OS_TRUE;
    }
}

/**
 * Insert into the pending ready list
 */
static void _OS_pending_ready_list_insert(TCB_t *tcb, int core_ID)
{
    _OS_task_list_append(tcb, &(OS_pending_ready_list[core_ID]));
}



