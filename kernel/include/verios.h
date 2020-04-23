#ifndef VERIOS_H
#define VERIOS_H

#include <stddef.h>

/* Standard includes */
#include <limits.h>
#include <stdint.h>

/* for likely and unlikely */
#include "esp_compiler.h"

/* Application specific configuration options. */
#include "freertos/FreeRTOSConfig.h"

/* Basic FreeRTOS definitions. */
#include "projdefs.h"

/* Definitions specific to the port being used. */
#include "portable.h"

/*******************************************************************************
* MACROS
*******************************************************************************/

#define OS_TRUE (uint8_t)1
#define OS_FALSE (uint8_t)0

#define OS_MAX_PRIORITIES 256

#define OS_MAX_MSG_QUEUE_SIZE INT_MAX

#ifndef OS_PRIVILEGE_BIT
    #define OS_PRIVILEGE_BIT ((uint8_t) 0x00)
#endif /* OS_PRIVILEGE_BIT */ 

/*******************************************************************************
* TYPEDEFS AND DATA STRUCTURES
*******************************************************************************/

typedef struct OSTaskControlBlock TCB_t;

typedef uint8_t OSBool_t;

typedef enum OS_error_codes {
    OS_NO_ERROR = 0,

    /* Task creation and deletion */
    OS_ERROR_RESERVED_PRIORITY,
    OS_ERROR_INVALID_STKSIZE,
    OS_ERROR_STACK_ALLOC,
    OS_ERROR_TCB_ALLOC,
    OS_ERROR_IDLE_DELETE,
    OS_ERROR_DOUBLE_DELETE,

    OS_ERROR_INVALID_TSK_STATE,
    OS_ERROR_INVALID_PRIO,
    OS_ERROR_INVALID_DLY,

    /* Attempting an operation that is not permitted based on the task's current state */
    OS_ERROR_DELETED_TASK,
    OS_ERROR_DELAYED_TASK,
    OS_ERROR_SUSPENDED_TASK,

    /* Attempting an operation that is not permitted based on the scheduler's current state */
    OS_ERROR_SCHEDULER_STOPPED,

    /* Msg Queue creation and deletion */
    OS_ERROR_INVALID_QUEUE_SIZE,
    OS_ERROR_QUEUE_ALLOC,
    OS_ERROR_QUEUE_NULL_PTR,

    OS_OTHER_ERROR
} OSError_t;

struct OSTaskListHeader {
    int num_tasks;
    TCB_t *head_ptr;
    TCB_t *tail_ptr;
};


typedef struct OSTaskListHeader WaitList_t;

/*******************************************************************************
* FUNCTION HEADERS
*******************************************************************************/


void OS_waitlist_insert_task(TCB_t *tcb, WaitList_t *waitlist);

#endif /* VERIOS_H */