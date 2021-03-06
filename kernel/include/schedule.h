#ifndef OS_SCHEDULE_H
#define OS_SCHEDULE_H

#include "task.h"
#include "list.h"
#include "verios.h"

/*******************************************************************************
* MACROS
*******************************************************************************/

#define OS_PRIO_MAP_SIZE (OS_MAX_PRIORITIES / 8)

/*******************************************************************************
* TYPEDEFS AND DATA STRUCTURES
*******************************************************************************/

/* CPU data information */
typedef struct OSCPU SchedCPU_t;

typedef struct OSTaskListHeader ReadyList_t;
typedef struct OSTaskListHeader DeletionList_t;
typedef struct OSTaskListHeader DelayedList_t;
typedef struct OSTaskListHeader SuspendedList_t;

typedef enum {
    OS_SCHEDULE_STATE_STOPPED,
    OS_SCHEDULE_STATE_RUNNING,
    OS_SCHEDULE_STATE_SUSPENDED
} OSScheduleState_t;

/* Stolen from FreeRTOS. Create my own version later */
typedef struct xTIME_OUT
{
	BaseType_t xOverflowCount;
	TickType_t xTimeOnEntering;
} TimeOut_t;

/*******************************************************************************
* FUNCTION HEADERS
*******************************************************************************/

void OS_schedule_init(void);

int OS_schedule_start(void);

void OS_schedule_stop(void);

void OS_schedule_suspend(void);

OSBool_t OS_schedule_resume(void);

void OS_schedule_switch_context(void);

int OS_schedule_add_task(TCB_t *new_tcb);

int OS_schedule_remove_task(TCB_t *old_tcb);

int OS_schedule_delay_task(TCB_t *tcb, TickType_t tick_delay);

int OS_schedule_suspend_task(TCB_t *tcb);

int OS_schedule_resume_task(TCB_t *tcb);

int OS_schedule_change_task_prio(TCB_t *tcb, TaskPrio_t new_prio);

void OS_schedule_raise_priority_mutex_holder(TCB_t *mutex_holder);

OSBool_t OS_schedule_revert_priority_mutex_holder(void * const mux_holder);

int OS_schedule_join_list_insert(TCB_t *waiter, TCB_t *tcb_to_join, TickType_t timeout);

void OS_schedule_waitlist_empty(WaitList_t *waitlist);

OSBool_t OS_schedule_process_tick(void);

TCB_t* OS_schedule_get_idle_tcb(int core_ID);

TCB_t* OS_schedule_get_current_tcb(void);

TCB_t* OS_schedule_get_current_tcb_from_core(int core_ID);

OSScheduleState_t OS_schedule_get_state(void);

TickType_t OS_schedule_get_tick_count(void);

void * OS_schedule_increment_task_mutex_count(void);

void OS_set_timeout_state( TimeOut_t * const pxTimeOut );

OSBool_t OS_schedule_check_for_timeout( TimeOut_t * const timeout, TickType_t * const ticks_to_wait);

void OS_schedule_place_task_on_event_list(List_t * const pxEventList, const TickType_t ticks_to_wait);

void OS_schedule_place_task_on_events_list_restricted(List_t * const pxEventList, const TickType_t ticks_to_wait);

void OS_schedule_place_task_on_unordered_events_list(List_t *pxEventList, const TickType_t item_value, const TickType_t ticks_to_wait);

int OS_schedule_remove_task_from_event_list(const List_t * const pxEventList);

int OS_schedule_remove_task_from_unordered_events_list(ListItem_t * pxEventListItem, const TickType_t item_value);

TickType_t OS_schedule_reset_task_event_item_value(void);


#endif /* OS_SCHEDULE_H */ 