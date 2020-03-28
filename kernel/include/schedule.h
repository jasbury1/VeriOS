#ifndef OS_SCHEDULE_H
#define OS_SCHEDULE_H

#include "task.h"

#define OS_PRIO_MAP_SIZE (OS_MAX_PRIORITIES / 8)

typedef struct OSTaskListHeader {
    int num_tasks;
    TCB_t *head_ptr;
    TCB_t *tail_ptr;
} ReadyList_t, DeletionList_t, DelayedList_t;

typedef enum {
    OS_SCHEDULE_NOT_STARTED,
    OS_SCHEDULE_RUNNING,
    OS_SCHEDULE_SUSPENDED
} OSScheduleState_t;

/**
 * FUNCTION HEADERS
 */

void OS_schedule_start(void);

void OS_schedule_stop(void);

void OS_schedule_suspend_all(void);

void OS_schedule_add_to_ready_list(TCB_t *new_tcb, int core_ID);

void OS_schedule_remove_from_ready_list(TCB_t *removed_tcb, int core_ID);

void OS_schedule_delay_task(const TickType_t tick_delay);

void OS_schedule_change_task_prio(TCB_t *tcb, TaskPrio_t new_prio);

OS_prio_t OS_schedule_get_task_prio(TCB_t *tcb);

OSBool_t OS_schedule_process_tick(void);

TCB_t* OS_schedule_get_idle_tcb(int core_ID);

TCB_t* OS_schedule_get_current_tcb(void);

OSScheduleState_t OS_schedule_get_state(void);

TickType_t OS_schedule_get_tick_count(void);

#endif /* OS_SCHEDULE_H */ 