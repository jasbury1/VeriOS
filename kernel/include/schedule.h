/**
 * 
 * 
 */
 
#ifndef OS_SCHEDULE_H
#define OS_SCHEDULE_H

#include "task.h"

#define OS_PRIO_MAP_SIZE (OS_MAX_PRIORITIES / 8)

typedef struct OSReadyListHeader {
    int num_tasks;
    TCB_t *head_ptr;
    TCB_t *tail_ptr;
} ReadyList_t;

#endif /* OS_SCHEDULE_H */ 