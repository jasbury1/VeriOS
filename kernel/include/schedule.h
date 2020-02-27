/**
 * 
 * 
 */
 
#ifndef OS_SCHEDULE_H
#define OS_SCHEDULE_H

#include "task.h"

typedef struct OSReadyListHeader {
    int num_tasks;
    TCB_t *head_ptr;
    TCB_t *tail_ptr;
} ReadyList_t;

#endif /* OS_SCHEDULE_H */ 