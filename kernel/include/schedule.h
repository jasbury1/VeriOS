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

int OS_schedule_start(void);

void OS_schedule_stop(void);

void OS_schedule_suspend(void);

OSBool_t OS_schedule_resume(void);

void OS_schedule_switch_context(void);

int OS_schedule_add_task(TCB_t *new_tcb);

int OS_schedule_remove_task(TCB_t *old_tcb);

int OS_schedule_delay_task(const TickType_t tick_delay);

int OS_schedule_change_task_prio(TCB_t *tcb, TaskPrio_t new_prio);

void OS_schedule_raise_priority_mutex_holder(TCB_t *mutex_holder);

OSBool_t OS_schedule_revert_priority_mutex_holder(void * const mux_holder);

TaskPrio_t OS_task_get_priority(TCB_t *tcb);

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


#endif /* OS_SCHEDULE_H */ 