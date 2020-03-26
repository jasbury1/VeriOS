

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


PRIVILEGED_DATA TCB_t * volatile OS_current_TCB[portNUM_PROCESSORS] = { NULL };

/* A counter for the number of tasks currently ready (not deleted) */
PRIVILEGED_DATA static volatile unsigned int OS_num_tasks = 0;

/* A counter for all tasks that have been deleted or pending deletion */
PRiVILEGED_DATA static volatile unsigned int OS_num_tasks_deleted = 0;

PRIVILEGED_DATA static volatile uint8_t OS_scheduler_running = OS_FALSE;
PRIVILEGED_DATA static volatile TickType_t xTickCount = ( TickType_t ) 0;

PRIVILEGED_DATA static portMUX_TYPE OS_schedule_mutex = portMUX_INITIALIZER_UNLOCKED

PRIVILEGED_DATA static TCB_t OS_idle_tcb_list[portNUM_PROCESSORS] = {NULL};

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
 * A list of tasks pending deletion (memory not yet freed).
 * Deletion should be carried out by IDLE for these tasks
 */
static DeletionList_t OS_deletion_pending_list;

/**
 * STATIC FUNCTION DEFINITIONS
 */

static void _OS_schedule_reset_prio_map(void);

static int _OS_schedule_get_highest_prio(void);

static void _OS_schedule_add_prio(TaskPrio_t new_prio);

static void _OS_schedule_remove_prio(TaskPrio_t old_prio);

static void _OS_schedule_ready_list_init(void);

static void _OS_schedule_ready_list_insert(TCB_t *new_tcb);

static void _OS_schedule_ready_list_remove(TCB_t *old_tcb);

static void _OS_schedule_deletion_pending_list_insert(TCB_t *tcb);

static void _OS_schedule_deletion_pending_list_remove(TCB_t *tcb);

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

/**
 * Zeros out the bit map storing priorities that are in use
 */
static void _OS_schedule_reset_prio_map(void){
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
static void _OS_schedule_add_prio(TaskPrio_t new_prio)
{
    TaskPrio_t index = (TaskPrio_t)((OS_MAX_PRIORITIES - new_prio) / 8);
    TaskPrio_t shift = (OS_MAX_PRIORITIES - new_prio) % 8;

    OS_ready_priorities_map[index] = OS_ready_priorities_map | ((TaskPrio_t)128 >> shift);
}

/**
 * Remove the entry in the bitmap corresponding to the given priority
 * Should only be done if no tasks use that priority anymore
 */
static void _OS_schedule_remove_prio(TaskPrio_t prio)
{
    TaskPrio_t index = (TaskPrio_t)((OS_MAX_PRIORITIES - prio) / 8);
    TaskPrio_t shift = (OS_MAX_PRIORITIES - prio) % 8;

    OS_ready_priorities_map[index] = OS_ready_priorities_map ^ ((TaskPrio_t)128 >> shift);
}

/**
 * Ensures that all entries in the ready list are reset
 */
static void _OS_schedule_ready_list_init(void)
{
    int index;
    for(index = 0; index < OS_MAX_PRIORITIES; ++i){
        OS_ready_list[i]->num_tasks = 0;
        OS_ready_list[i]->head_ptr = NULL;
        OS_ready_list[i]->tail_ptr = NULL;
    }
}

/**
 * Responsible for placing the tcb in the ready list
 * Application code should not call! This is a helper for OS_add_task_to_ready_list
 */
static void _OS_schedule_ready_list_insert(TCB_t *new_tcb)
{
    /* If this is the first task in this priority bracket */
    int prio = new_tcb->priority;

    /* First entry at given priority */
    if(OS_ready_list[prio].head_ptr == NULL){
        OS_ready_list[prio].head_ptr = new_tcb;
        OS_ready_list[prio].tail_ptr = new_tcb;
        OS_ready_list[prio].num_tasks = 1;
        _OS_schedule_add_prio(new_tcb->priority);
        return;
    }
    /* Add to round-robin at given priority */
    OS_ready_list[prio].tail_ptr.next_ptr = new_tcb;
    new_tcb->prev_ptr = OS_ready_list[prio].tail_ptr;
    OS_ready_list[prio].tail_ptr = new_tcb;
    OS_ready_list[prio].num_tasts++;
}

static void _OS_schedule_ready_list_remove(TCB_t *old_tcb)
{
    TCB_t *tcb_next = old_tcb->next_ptr;
    TCB_t *tcb_prev = old_tcb->prev_ptr;
    
    old_tcb->next_prt = NULL;
    old_tcb->prev_ptr = NULL;
    OS_ready_list[old_tcb->priority].num_tasks--;

    /* Is this tcb at the head of the list? */
    if(tcb_prev == NULL){
        /* Is this tcb the only entry at this priority level? */
        if(tcb_next == NULL){
            OS_ready_list[old_tcb->priority].head_ptr = NULL;
            OS_ready_list[old_tcb->priority].tail_ptr = NULL;

            /* Remove this priority from the bitmap */
            _OS_schedule_remove_prio(old_tcb->priority);
        }
        /* Update the new head pointer for this priority and decrement task counter */
        else {
            tcb_next->prev_ptr = NULL;
            OS_ready_list[old_tcb->priority].head_ptr = tcb_next
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

static void _OS_schedule_deletion_pending_list_insert(TCB_t *tcb)
{
    /* First entry */
    if(OS_deletion_pending_list.head_ptr == NULL){
        OS_deletion_pending_list.head_ptr = tcb;
        OS_deletion_pending_list.tail_ptr = tcb;
        OS_deletion_pending_list.num_tasks = 1;
        return;
    }
    /* Add to round-robin at given priority */
    OS_deletion_pending_list.tail_ptr.next_ptr = tcb;
    tcb->prev_ptr = OS_deletion_pending_list.tail_ptr;
    OS_deletion_pending_list.tail_ptr = tcb;
    OS_deletion_pending_list.num_tasts++;
}

static void _OS_schedule_deletion_pending_list_remove(TCB_t *tcb)
{
    /* TODO */
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
        _OS_schedule_reset_prio_map();
        _OS_schedule_ready_list_init();
    }
    
    /* If there is no affinity with which core it is placed on */
    if(core_ID == CORE_NO_AFFINITY) {
        /* Place it on the single existing core if we don't have multiple cores */
        core_ID = portNUM_PROCESSORS == 1 ? 0 : core_ID;
        /* Otherwise place it on a core. TODO: Better logic here in the future. Optimize */
        core_ID = xPortGetCoreID();
    }

    /* If nothing is running on this core then put dat task there */
    if(OS_current_tcb[core_ID] == NULL ) {
        OS_current_tcb[core_ID] = pxNewTCB;
    }

    /* Make this task the current if its priority is the highest and the scheduler isn't running */
    else if(OS_scheduler_running == OS_FALSE){
        if(OS_current_tcb[core_ID] == NULL || OS_current_tcb[coreID]->priority <= new_tcb->priority) {
            OS_current_tcb[core_ID] = new_tcb;
        }
    }
    
    /* Increase the task count and update the ready list */
    ++OS_num_tasks;
    _OS_schedule_ready_list_insert(new_tcb);

    if(OS_scheduler_running == OS_FALSE) {
        portEXIT_CRITICAL(&OS_schedule_mutex)
        return;
    }

	/* Scheduler is running. Check to see if we should run the task now */
	if(OS_current_tcb[core_ID] == NULL || OS_current_tcb[core_ID]->priority < new_tcb->priority) {
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
    uint8_t ready_to_delete = OS_TRUE;
    int i;

    /* Remove the task from the ready list */
    _OS_schedule_ready_list_remove(old_tcb);

    /* Update task counters */
    OS_num_tasks--;
    OS_num_tasks_deleted++;

    /* If task is waiting on an event, remove it from the wait list */
    if(OS_TRUE) {
       /* TODO */
    }

    /* Idle must delete this task if it is not on this core */
    if(core_ID != xPortGetCoreID()){
        ready_to_delete = OS_FALSE:
    }
    /* Idle must delete this task if it is running on any core */
    for(i = 0; i < portNUM_PROCESSORS; ++i){
        if(OS_currrent_TCB[i] == old_tcb){
            ready_to_delete = OS_FALSE;
        }
    }

    if(ready_to_delete == OS_TRUE){
        old_tcb->task_state = OS_TASK_STATE_READY_TO_DELETE;
    }
    else {
        old_tcb->task_state = OS_TASK_STATE_PENDING_DELETION;
        _OS_schedule_deletion_pending_list_insert(old_tcb);
    }

    /* Force a reschedule if the conditions require it */
    if(OS_scheduler_running == OS_TRUE){
        /* If the task is running on this core */
        if(old_tcb == OS_current_TCB[core_ID]){
            portYIELD_WITHIN_API();
        }
        /* Yield if the task was running on any other core */
        for(i = 0; i < portNUM_PROCESSORS; ++i){
            if(OS_currrent_TCB[i] == old_tcb){
                vPortYieldOtherCore(i);
            }
        }
    }

    portEXIT_CRITICAL(&OS_schedule_mutex);
}

void OS_schedule_start(void)
{
    int i;
    int ret_val;

    for(i = 0; i < portNUM_PROCESSORS; ++i) {
        ret_val = OS_task_create(OS_idle_task, (void *)NULL, "IDLE", 0, OS_IDLE_STACK_SIZE, 0, i, &OS_idle_tcb_list[i]);
        if(ret_val != 0){
            /* TODO Handle error */
        }
    }
    ret_val = xTimerCreateTimerTask();
    if(ret_val != 1){
        /* TODO Handle error */
    }

    portDISABLE_INTERRUPTS();
    xTickCount = ( TickType_t ) 0U;
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
    xSchedulerRunning = pdFALSE;
    vPortEndScheduler();
}



TCB_t* OS_schedule_get_idle_tcb(int core_ID)
{
    return &OS_idle_tcb_list[core_ID];
}

TCB_t* OS_schedule_get_current_tcb(void)
{
    TCB_t *current_tcb;

    unsigned state = portENTER_CRITICAL_NESTED();
    current_tcb = &(OS_current_TCB[xPortGetCoreID()]);
    portEXIT_CRITICAL_NESTED(state);

    return current_tcb;
}

void OS_schedule_update(void)
{
}
