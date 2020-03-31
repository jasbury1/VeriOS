

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


PRIVILEGED_DATA TCB_t * volatile OS_current_TCB[portNUM_PROCESSORS] = { NULL };

/* A counter for the number of tasks currently ready (not deleted) */
PRIVILEGED_DATA static volatile unsigned int OS_num_tasks = 0;

/* A counter for all tasks that have been deleted or pending deletion */
PRIVILEGED_DATA static volatile unsigned int OS_num_tasks_deleted = 0;

/* A counter for tasks that have been delayed and placed on a list */
PRIVILEGED_DATA static volatile unsigned int OS_num_tasks_delayed = 0;
PRIVILEGED_DATA static volatile TickType_t OS_next_task_unblock_time = portMAX_DELAY;

PRIVILEGED_DATA static volatile OSBool_t OS_scheduler_running = OS_FALSE;
PRIVILEGED_DATA static volatile TickType_t OS_tick_counter = (TickType_t)0;
PRIVILEGED_DATA static volatile TickType_t OS_pending_ticks = (TickType_t)0;

PRIVILEGED_DATA static portMUX_TYPE OS_schedule_mutex = portMUX_INITIALIZER_UNLOCKED;

PRIVILEGED_DATA static TCB_t * OS_idle_tcb_list[portNUM_PROCESSORS] = { NULL };

/* Keep track of if schedulers are suspended (OS_TRUE) or not (OS_FALSE) */
PRIVILEGED_DATA static volatile OSBool_t OS_suspended_schedulers_list[portNUM_PROCESSORS] = {OS_FALSE};
/* Keep track of if yields are pending (OS_TRUE) or not (OS_FALSE) on a given processor */
PRIVILEGED_DATA static volatile OSBool_t OS_yield_pending_list[portNUM_PROCESSORS] = {OS_FALSE};

/**
 * Bitmap of priorities in use.
 * Broken up into an array of 8 bit integers where each bit represents if a priority is in use
 * The map is designed backwards. 
 * The first bit corresponds to the highest priority (highest number)
 * The last bit is priority 0 (reserved for Idle)
 */
static uint8_t OS_ready_priorities_map[OS_PRIO_MAP_SIZE];

/**
 * The Ready list of all tasks ready to be run
 * Each index corresponds to a priority. Index 0 is priority 0 (reserved for Idle)
 */
static ReadyList_t OS_ready_list[OS_MAX_PRIORITIES];

/**
 * The list of tasks that were scheduled while the scheduler was suspended
 */

static ReadyList_t OS_pending_ready_list[portNUM_PROCESSORS];

/**
 * A list of tasks pending deletion (memory not yet freed).
 * Deletion should be carried out by IDLE for these tasks
 */
static DeletionList_t OS_deletion_pending_list;

/**
 * The lists of delayed tasks
 * Index 0 is the regular list, index 1 is a list for overflowed delays.
 * Both indece's lists are sorted by ascending wake-up time
 */
static volatile DelayedList_t OS_delayed_list[2] = {{0, NULL, NULL}};

/**
 * STATIC FUNCTION DEFINITIONS
 */

static void _OS_bitmap_reset_prios(void);

static int _OS_schedule_get_highest_prio(void);

static void _OS_bitmap_add_prio(TaskPrio_t new_prio);

static void _OS_bitmap_remove_prio(TaskPrio_t old_prio);

static void _OS_ready_list_init(void);

static void _OS_ready_list_insert(TCB_t *new_tcb);

static void _OS_ready_list_remove(TCB_t *old_tcb);

static void _OS_deletion_pending_list_insert(TCB_t *tcb);

static void _OS_deletion_pending_list_remove(TCB_t *tcb);

static void _OS_delayed_list_insert(TCB_t *tcb, OSBool_t use_overflow);

static void _OS_delayed_list_remove(TCB_t *tcb, OSBool_t use_overflow);

static OSBool_t _OS_delayed_list_wakeup_next_task(void);

static void _OS_delayed_tasks_cycle_overflow(void);

static void _OS_pending_ready_list_schedule_next_task(void);

static void _OS_update_next_task_unblock_time(void);

/**
 * The IDLE Task
 * TODO: Rewrite and move into Task.c
 */
static portTASK_FUNCTION(OS_idle_task, idle_task_params)
{
    (void) idle_task_params;
    for(;;) {
        /* DONT WORRY ABOUT DECREMENTING NUMBER OF TASKS HERE
        This right now will automaticallly be done in the remove from ready list func
        */
    }
}

void OS_schedule_start(void)
{
    int i;
    int ret_val;

    for(i = 0; i < portNUM_PROCESSORS; ++i) {
        ret_val = OS_task_create(OS_idle_task, (void *)NULL, "IDLE", 0, OS_IDLE_STACK_SIZE, 0, i, OS_idle_tcb_list[i]);
        if(ret_val != 0){
            /* TODO Handle error */
        }
    }
    ret_val = xTimerCreateTimerTask();
    if(ret_val != 1){
        /* TODO Handle error */
    }

    portDISABLE_INTERRUPTS();
    OS_tick_counter = ( TickType_t ) 0U;
    portCONFIGURE_TIMER_FOR_RUN_TIME_STATS();
    OS_scheduler_running = OS_TRUE;

    if (xPortStartScheduler() != OS_FALSE) {
        /* Should not reach here as if the scheduler is running the
			function will not return. */
    }
    else {
        /* Should only reach here if a task calls xTaskEndScheduler(). */
    }
}

void OS_schedule_stop()
{
    /* Stop the scheduler interrupts and call the portable scheduler end
	routine so the original ISRs can be restored if necessary.  The port
	layer must ensure interrupts enable	bit is left in the correct state. */
    portDISABLE_INTERRUPTS();
    OS_scheduler_running = OS_FALSE;
    vPortEndScheduler();
}

void OS_schedule_suspend_all(void)
{
    unsigned state;
    state = portENTER_CRITICAL_NESTED();
    /* Increment rather than just set to OS_TRUE since suspend calls can be nested */
    OS_suspended_schedulers_list[xPortGetCoreID()]++;
    portEXIT_CRITICAL_NESTED(state);
}

/**
 * Resume the scheduler. Does not resume individual suspended tasks.
 * 
 * Returns whether a context switch is occured or not
 */
OSBool_t OS_schedule_resume_all(void)
{
    OSBool_t yield_occured = OS_FALSE;

    /* TODO: Use OS_schedule_get_state to make sure the scheduler is NOT RUNNING */
    /* TODO: Ensure that the suspended int is larger than zero */

    portENTER_CRITICAL(&OS_schedule_mutex);

    OS_suspended_schedulers_list[xPortGetCoreID()]--;
    
    /* Nothing to do if we are still in nested suspension or have no tasks */
    if(OS_suspended_schedulers_list[xPortGetCoreID()] == OS_TRUE ||
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

    if( OS_yield_pending_list[xPortGetCoreID()] == OS_TRUE ) {
		yield_occured = OS_TRUE;
        portYIELD_WITHIN_API();
	}

    portEXIT_CRITICAL(&OS_schedule_mutex);
    return yield_occured;
}

/**
 * The main API function for adding to a ready list!
 * Handles all of the stuff
 */
void OS_schedule_add_to_ready_list(TCB_t *new_tcb, int core_ID)
{
    /* TODO : Ensure that the coreID is not invalid / out of range */
    portENTER_CRITICAL(&OS_schedule_mutex);

    /* Do the setup for the ready lists if this is the first task */
    if(OS_num_tasks == 0){
        _OS_bitmap_reset_prios();
        _OS_ready_list_init();
    }
    
    /* If there is no affinity with which core it is placed on */
    if(core_ID == CORE_NO_AFFINITY) {
        /* Place it on the single existing core if we don't have multiple cores */
        core_ID = portNUM_PROCESSORS == 1 ? 0 : core_ID;
        /* Otherwise place it on a core. TODO: Better logic here in the future. Optimize */
        core_ID = xPortGetCoreID();
    }

    /* If nothing is running on this core then put dat task there */
    if(OS_current_TCB[core_ID] == NULL ) {
        OS_current_TCB[core_ID] = new_tcb;
    }

    /* Make this task the current if its priority is the highest and the scheduler isn't running */
    else if(OS_scheduler_running == OS_FALSE){
        if(OS_current_TCB[core_ID] == NULL || OS_current_TCB[core_ID]->priority <= new_tcb->priority) {
            OS_current_TCB[core_ID] = new_tcb;
        }
    }
    
    /* Increase the task count and update the ready list */
    ++OS_num_tasks;
    _OS_ready_list_insert(new_tcb);

    new_tcb->task_state = OS_TASK_STATE_READY;

    if(OS_scheduler_running == OS_FALSE) {
        portEXIT_CRITICAL(&OS_schedule_mutex);
        return;
    }

	/* Scheduler is running. Check to see if we should run the task now */
	if(OS_current_TCB[core_ID] == NULL || OS_current_TCB[core_ID]->priority < new_tcb->priority) {
		if( core_ID == xPortGetCoreID() )
		{
			portYIELD_WITHIN_API();
		}
		else {
            /* See if we need to interrupt the other core to run the task now */
            vPortYieldOtherCore(core_ID);
		}
	}

    portEXIT_CRITICAL(&OS_schedule_mutex);
}

void OS_schedule_remove_from_ready_list(TCB_t *old_tcb, int core_ID)
{
    portENTER_CRITICAL(&OS_schedule_mutex);
    OSBool_t ready_to_delete = OS_TRUE;
    int i;

    /* Remove the task from the ready list */
    /* TODO !!!!! It might not be on the ready list. Maybe its on the
    delayed task list??? */
    /* If it is on a delayed task list, also call _OS_update_next_task_unblock_time */
    _OS_ready_list_remove(old_tcb);

    /* Update task counters */
    OS_num_tasks--;
    OS_num_tasks_deleted++;

    /* If task is waiting on an event, remove it from the wait list */
    if(OS_TRUE) {
       /* TODO */
    }

    /* Idle must delete this task if it is not on this core */
    if(core_ID != xPortGetCoreID()){
        ready_to_delete = OS_FALSE;
    }
    /* Idle must delete this task if it is running on any core */
    for(i = 0; i < portNUM_PROCESSORS; ++i){
        if(OS_current_TCB[i] == old_tcb){
            ready_to_delete = OS_FALSE;
        }
    }

    if(ready_to_delete == OS_TRUE){
        old_tcb->task_state = OS_TASK_STATE_READY_TO_DELETE;
    }
    else {
        old_tcb->task_state = OS_TASK_STATE_PENDING_DELETION;
        _OS_deletion_pending_list_insert(old_tcb);
    }

    /* Force a reschedule if the conditions require it */
    if(OS_scheduler_running == OS_TRUE){
        /* If the task is running on this core */
        if(old_tcb == OS_current_TCB[core_ID]){
            portYIELD_WITHIN_API();
        }
        /* Yield if the task was running on any other core */
        for(i = 0; i < portNUM_PROCESSORS; ++i){
            if(OS_current_TCB[i] == old_tcb){
                vPortYieldOtherCore(i);
            }
        }
    }

    portEXIT_CRITICAL(&OS_schedule_mutex);
}

/**
 * Delay the current task the amount of ticks specified.
 * Delay values of 0 or below zero just force a reschedule
 */
void OS_schedule_delay_task(const TickType_t tick_delay)
{
    TickType_t wakeup_time;
    TCB_t *current_tcb;
    OSBool_t use_overflow_list = OS_FALSE;
    
    /* TODO: Make sure the scheduler is running. Return error otherwise */
    /* TODO: make sure tick_delay is NOT negative */
    
    portENTER_CRITICAL(&OS_schedule_mutex);
    wakeup_time = tick_delay + OS_tick_counter;
    current_tcb = OS_current_TCB[xPortGetCoreID()];

    if(wakeup_time < OS_tick_counter){
        use_overflow_list = OS_TRUE;
    }

    /* Remove ourselves from the Ready List */
    _OS_ready_list_remove(current_tcb);

    /* Set the state of the current TCB to delayed */
    current_tcb->task_state = OS_TASK_STATE_DELAYED;

    /* Add to a list of delayed tasks */
    current_tcb->delay_wakeup_time = wakeup_time;
    _OS_delayed_list_insert(current_tcb, use_overflow_list);

    portEXIT_CRITICAL(&OS_schedule_mutex);

    portYIELD_WITHIN_API();
}

/**
 * Notify the kernel that a tick has occured.
 * Delays and more must be updated due to this tick event.
 * 
 * Returns whether or not a context switch is required
 */
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
        if (xPortGetCoreID() == 1 ) {
			return OS_TRUE;
		}
    }

    /* Any core can unwind pending ticks while resuming all tasks */
    if( OS_suspended_schedulers_list[ xPortGetCoreID() ] == OS_TRUE) {
        ++OS_pending_ticks;
        return context_switch_required;
    }

    portENTER_CRITICAL_ISR(&OS_schedule_mutex);

    ++OS_tick_counter;
    if(OS_tick_counter == (TickType_t)0){
        _OS_delayed_tasks_cycle_overflow();
    }

    /* Wakeup any tasks whose timers have expired */
    while((OS_delayed_list[0].head_ptr != NULL) && 
          (OS_delayed_list[0].head_ptr->delay_wakeup_time <= OS_tick_counter)) {
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

/**
 * Change the priority of a given task
 */
void OS_schedule_change_task_prio(TCB_t *tcb, TaskPrio_t new_prio)
{
    OSBool_t context_switch_required = OS_FALSE;
    TaskPrio_t old_prio;

    /* TODO: Assert the new priority is valid */
    /* TODO: Assert the task has not been deleted */
    
    portENTER_CRITICAL(&OS_schedule_mutex);
    
    /* If the tcb is null, get the current tcb */
    if(tcb == NULL){
        tcb = OS_schedule_get_current_tcb();
    }
    old_prio = tcb->priority;
    tcb->priority = new_prio;

    /* We raised a task's priority but its not the current task*/
    if(new_prio > old_prio && tcb != OS_current_TCB[xPortGetCoreID()]) {
        /* Should run on this core right now */
        if(tcb->core_ID == xPortGetCoreID() && new_prio >= OS_current_TCB[xPortGetCoreID()]->priority) {
            context_switch_required = OS_TRUE;
        }
        /* Might run on the other core right now */
        else if(tcb->core_ID != xPortGetCoreID(){
            vPortYieldOtherCore(tcb->core_ID);
        }        
    }

    /* Reducing the priority of the current task */
    if(old_prio > new_prio && tcb == OS_current_TCB[xPortGetCoreID()]) {
        context_switch_required = OS_TRUE;
    }

    if(tcb->task_state == OS_TASK_STATE_READY) {
        /* remove from the old ready list and add to a new one */
        _OS_ready_list_remove(tcb);
        _OS_ready_list_insert(tcb);
    }

    if(context_switch_required == OS_TRUE){
        portYIELD_WITHIN_API();
    }

    portEXIT_CRITICAL(&OS_schedule_mutex);
}

TaskPrio_t OS_schedule_get_task_prio(TCB_t *tcb)
{
    TaskPrio_t result;
    portENTER_CRITICAL(&OS_schedule_mutex);
    result = tcb->priority;
    portENTER_CRITICAL(&OS_schedule_mutex);
    return result;
}

static void _OS_delayed_tasks_cycle_overflow(void)
{
    /* TODO: ERROR if the first list is not empty */
    OS_delayed_list[0].head_ptr = OS_delayed_list[1].head_ptr;
    OS_delayed_list[0].tail_ptr = OS_delayed_list[1].tail_ptr; 
    OS_delayed_list[0].num_tasks = OS_delayed_list[1].num_tasks;

    OS_delayed_list[1].num_tasks = 0;
    OS_delayed_list[1].head_ptr = NULL;
    OS_delayed_list[1].tail_ptr = NULL;

    _OS_update_next_task_unblock_time();
}

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
    if(OS_delayed_list[0].head_ptr == NULL){
        OS_next_task_unblock_time = portMAX_DELAY;
        return;
    }
    OS_next_task_unblock_time = OS_delayed_list[0].head_ptr->delay_wakeup_time;
}

/**
 * Zeros out the bit map storing priorities that are in use
 */
static void _OS_bitmap_reset_prios(void){
    int i;
    for(i = 0; i < OS_PRIO_MAP_SIZE; ++i){
        OS_ready_priorities_map[i] = (uint8_t)0;
    }
}

/**
 * Return the highest priority that is currently assigned to any task
 */
static int _OS_schedule_get_highest_prio(void){
    uint8_t val;
    int leading_zeros = 0;
    int map_index = -1;

    while(OS_ready_priorities_map[++map_index] == (uint8_t)0 && map_index < OS_PRIO_MAP_SIZE);

    if(map_index == OS_PRIO_MAP_SIZE){
        /* TODO */
        /* No priorities in use in the map */
    }

    /* Count the leading zeros in the bit map index that contains a priority */
    val = OS_ready_priorities_map[map_index];
    while(!(val & (1 << 7))){
        val = (val << 1);
        ++leading_zeros;
    }

    return((OS_MAX_PRIORITIES - 1) - ((map_index * 8) + leading_zeros));
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
    /* If this is the first task in this priority bracket */
    int prio = new_tcb->priority;

    /* First entry at given priority */
    if(OS_ready_list[prio].num_tasks == 0){
        OS_ready_list[prio].head_ptr = new_tcb;
        OS_ready_list[prio].tail_ptr = new_tcb;
        OS_ready_list[prio].num_tasks = 1;
        _OS_bitmap_add_prio(new_tcb->priority);
        return;
    }
    /* Add to round-robin at given priority */
    OS_ready_list[prio].tail_ptr->next_ptr = new_tcb;
    new_tcb->prev_ptr = OS_ready_list[prio].tail_ptr;
    OS_ready_list[prio].tail_ptr = new_tcb;
    OS_ready_list[prio].num_tasks++;
}

static void _OS_ready_list_remove(TCB_t *old_tcb)
{
    TCB_t *tcb_next = old_tcb->next_ptr;
    TCB_t *tcb_prev = old_tcb->prev_ptr;
    
    old_tcb->next_ptr = NULL;
    old_tcb->prev_ptr = NULL;
    OS_ready_list[old_tcb->priority].num_tasks--;

    /* Is this tcb at the head of the list? */
    if(tcb_prev == NULL){
        /* Is this tcb the only entry at this priority level? */
        if(tcb_next == NULL){
            OS_ready_list[old_tcb->priority].head_ptr = NULL;
            OS_ready_list[old_tcb->priority].tail_ptr = NULL;

            /* Remove this priority from the bitmap */
            _OS_bitmap_remove_prio(old_tcb->priority);
        }
        /* Update the new head pointer for this priority and decrement task counter */
        else {
            tcb_next->prev_ptr = NULL;
            OS_ready_list[old_tcb->priority].head_ptr = tcb_next;
        }
        return;
    }

    /* This tcb is not the head of the list */
    tcb_prev->next_ptr = tcb_next;

    /* Adjust tail if removing from the tail */
    if(tcb_next == NULL) {
        OS_ready_list[old_tcb->priority].tail_ptr = tcb_prev;
    }
    else {
        tcb_next->prev_ptr = tcb_prev;
    }
    return;
}

static void _OS_deletion_pending_list_insert(TCB_t *tcb)
{
    /* First entry */
    if(OS_deletion_pending_list.num_tasks == 0){
        OS_deletion_pending_list.head_ptr = tcb;
        OS_deletion_pending_list.tail_ptr = tcb;
        OS_deletion_pending_list.num_tasks = 1;
        return;
    }
    /* Add to round-robin at given priority */
    OS_deletion_pending_list.tail_ptr->next_ptr = tcb;
    tcb->prev_ptr = OS_deletion_pending_list.tail_ptr;
    OS_deletion_pending_list.tail_ptr = tcb;
    OS_deletion_pending_list.num_tasks++;
}

static void _OS_deletion_pending_list_remove(TCB_t *tcb)
{
    /* TODO */
}

static void _OS_delayed_list_insert(TCB_t *tcb, OSBool_t use_overflow)
{
    DelayedList_t delayed_list;
    TCB_t *tcb1;
    TCB_t *tcb2;

    ++OS_num_tasks_delayed;

    if(use_overflow == OS_TRUE) {
        delayed_list = OS_delayed_list[1];
    }
    else {
        delayed_list = OS_delayed_list[0];
    }

    /* Update the task counter for this delayed list */
    delayed_list.num_tasks++;

    /* First entry in the delayed list */
    if(delayed_list.head_ptr == NULL) {
        delayed_list.head_ptr = tcb;
        delayed_list.tail_ptr = tcb;
        if(use_overflow == OS_FALSE){
            _OS_update_next_task_unblock_time();
        }
        return;
    }
    /* This task is the first one to get woken up */
    if(delayed_list.head_ptr->delay_wakeup_time >= tcb->delay_wakeup_time) {
        tcb->prev_ptr = NULL;
        tcb->next_ptr = delayed_list.head_ptr;
        delayed_list.head_ptr->prev_ptr = tcb;
        delayed_list.head_ptr = tcb;
        return;
    }
    /* This task is the last one to get woken up */
    if(delayed_list.tail_ptr->delay_wakeup_time <= tcb->delay_wakeup_time) {
        tcb->next_ptr = NULL;
        tcb->prev_ptr = delayed_list.tail_ptr;
        delayed_list.tail_ptr->next_ptr = tcb;
        delayed_list.tail_ptr = tcb;
        return;
    }
    /* Else, find the spot to place this task */
    tcb1 = delayed_list.head_ptr;
    while(tcb1->delay_wakeup_time <= tcb->delay_wakeup_time){
        tcb1 = tcb1->next_ptr;
    }
    tcb2 = tcb1->prev_ptr;

    tcb2->next_ptr = tcb;
    tcb1->prev_ptr = tcb;
    tcb->prev_ptr = tcb2;
    tcb->next_ptr = tcb1;
}

static void _OS_delayed_list_remove(TCB_t *tcb, OSBool_t use_overflow)
{
    /* TODO */

    /* Every time we remove, make sure to update the OS_next_task_wakeup thing */
}


/**
 * Wake up the next task on the delayed list.
 * This only deals with the main delayed list, we won't ever wakeup from the overflow list.
 * 
 * Returns True if a context switch is required after the wakeup. False otherwise.
 */
static OSBool_t _OS_delayed_list_wakeup_next_task(void)
{
    /* TODO: assert the head of the list is not empty */
    
    DelayedList_t delayed_list = OS_delayed_list[0];
    TCB_t *woken_task = delayed_list.head_ptr;
    OSBool_t context_switch_required = OS_FALSE;

    /* If this is the only entry on the delayed list */
    if(delayed_list.num_tasks == 1){
        delayed_list.head_ptr = NULL;
        delayed_list.tail_ptr = NULL;
        delayed_list.num_tasks = 0;
    } 
    else {
        delayed_list.head_ptr = woken_task->next_ptr;
        delayed_list.head_ptr->prev_ptr = NULL;
        delayed_list.num_tasks--;
    }

    /* Does the woken task have a higher priority than the running task? */
    if(woken_task->priority >= OS_current_TCB[xPortGetCoreID()]->priority){
        context_switch_required = OS_TRUE;
    }
    /* TODO: Remove from events list if it is on one */

    /* place task on the ready list */
    _OS_ready_list_insert(woken_task);
    woken_task->task_state = OS_TASK_STATE_READY;

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
    ReadyList_t pending_ready_list = OS_pending_ready_list[xPortGetCoreID()];
    TCB_t *scheduled_tcb = pending_ready_list.head_ptr;

    /* If this is the only entry on the pending ready list */
    if(pending_ready_list.num_tasks == 1){
        pending_ready_list.head_ptr = NULL;
        pending_ready_list.tail_ptr = NULL;
        pending_ready_list.num_tasks = 0;        
    }
    else {
        pending_ready_list.head_ptr = scheduled_tcb->next_ptr;
        pending_ready_list.head_ptr->prev_ptr = NULL;
        pending_ready_list.num_tasks--;
    }

    /* Place on the ready list */
    _OS_ready_list_insert(scheduled_tcb);
    scheduled_tcb->task_state = OS_TASK_STATE_READY;

    if(scheduled_tcb->priority >= OS_current_TCB[xPortGetCoreID()]->priority) {
        OS_yield_pending_list[xPortGetCoreID()] = OS_TRUE;
    }
}

TCB_t* OS_schedule_get_idle_tcb(int core_ID)
{
    TCB_t *idle = OS_idle_tcb_list[core_ID];
    return idle;
}

TCB_t* OS_schedule_get_current_tcb(void)
{
    TCB_t *current_tcb;

    unsigned state = portENTER_CRITICAL_NESTED();
    current_tcb = OS_current_TCB[xPortGetCoreID()];
    portEXIT_CRITICAL_NESTED(state);

    return current_tcb;
}

OSScheduleState_t OS_schedule_get_state(void)
{
    unsigned state;
    OSScheduleState_t current_state;
    
    state = portENTER_CRITICAL_NESTED();
    if (OS_scheduler_running == OS_FALSE) {
        current_state = OS_SCHEDULE_NOT_STARTED;
    }
    else {
        if (OS_suspended_schedulers_list[xPortGetCoreID()] == OS_FALSE) {
            current_state = OS_SCHEDULE_RUNNING;
        }
        else {
            current_state = OS_SCHEDULE_SUSPENDED;
        }
    }
    portEXIT_CRITICAL_NESTED(state);
    return current_state;
}

TickType_t OS_schedule_get_tick_count(){
    return OS_tick_counter;
}