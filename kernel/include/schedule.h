#ifndef OS_SCHEDULE_H
#define OS_SCHEDULE_H

#include "task.h"

#define OS_PRIO_MAP_SIZE (OS_MAX_PRIORITIES / 8)

typedef struct OSReadyListHeader {
    int num_tasks;
    TCB_t *head_ptr;
    TCB_t *tail_ptr;
} ReadyList_t;

/**
 * FUNCTION HEADERS
 */

void OS_schedule_add_to_ready_list(TCB_t *new_tcb, int core_ID);

void OS_schedule_remove_from_ready_list(TCB_t *removed_tcb, int core_ID);

void OS_schedule_start(void);

void OS_schedule_stop(void);

void OS_schedule_update(void);

TCB_t* OS_schedule_get_idle_tcb(int core_ID);

#endif /* OS_SCHEDULE_H */ 