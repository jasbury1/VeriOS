#ifndef VERIOS_UTIL_H
#define VERIOS_UTIL_H

#include <stddef.h>

/* Standard includes */
#include <limits.h>
#include <stdint.h>

/* Application specific configuration options. */
#include "verios.h"


/*******************************************************************************
* TYPEDEFS AND DATA STRUCTURES
*******************************************************************************/

/*******************************************************************************
* FUNCTION HEADERS
*******************************************************************************/


void _OS_waitlist_append(TCB_t *tcb, WaitList_t *waitlist);

void _OS_waitlist_remove(TCB_t *tcb);

TCB_t * _OS_waitlist_pop_head(WaitList_t *waitlist);

void _OS_task_list_remove(TCB_t* tcb, volatile struct OSTaskListHeader * task_list);

void _OS_task_list_append(TCB_t *tcb, volatile struct OSTaskListHeader * task_list);

#endif /* VERIOS_UTIL_H */