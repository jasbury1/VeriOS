#ifndef OS_TASK_H
#define OS_TASK_H


#include <limits.h>
#include <stdint.h>

#include "freertos/portmacro.h"
#include "verios.h"
#include "msg_queue.h"
#include "list.h"


/*******************************************************************************
* MACROS
*******************************************************************************/

#define OS_TID_TABLE_INITIAL_SIZE 64

#define CORE_NO_AFFINITY INT_MAX

#define OS_STACK_FILL_BYTE	( 0xa5U )

#define OS_MAX_TASK_NAME 32

#define OS_IDLE_STACK_SIZE configIDLE_TASK_STACK_SIZE
#define OS_IDLE_PRIORITY (uint8_t)0
#define OS_IDLE_NAME ((const char* const)"IDLE")

#define OS_CURRENT_TASK (Tid_t)-1

/*******************************************************************************
* TYPEDEFS AND DATA STRUCTURES
*******************************************************************************/

/* Specity the size of the integer for a task priority */
typedef uint8_t TaskPrio_t;

/* The function pointer representing a task function */
typedef void (*TaskFunc_t)( void * );

/* If we ever want to pass TCBs around anonymously, cast as TaskHandle_t */
typedef void * TaskHandle_t;

typedef enum {
    OS_TASK_STATE_RUNNING,
    OS_TASK_STATE_PENDING_READY,
    OS_TASK_STATE_READY,
    OS_TASK_STATE_DELAYED,
    OS_TASK_STATE_SUSPENDED,
    OS_TASK_STATE_PENDING_DELETION,
    OS_TASK_STATE_READY_TO_DELETE
} OSTaskState_t;

typedef void *TLSPtr_t;
typedef void (*TLSPtrDeleteCallback_t)(int, void *);

/**
 * Defines the memory ranges allocated to the task when an MPU is used.
 */
typedef struct xMEMORY_REGION
{
	void *base_address;
	uint32_t length_in_bytes;
	uint32_t parameters;
} MemoryRegion_t;

/**
 * The Task Control Block
 * Kernel bookkeeping on each task created
 * 
 * Should NEVER be used by user code
 */
struct OSTaskControlBlock
{
    /* Pointer to last element pushed to stack */
    volatile StackType_t *stack_top;

    /* MPU Settings */
    xMPU_SETTINGS MPU_settings;

    /* The task's ID */
    Tid_t tid;

    /* Set to OS_TRUE if the task was statically allocated and doesn't require freeing */
    OSBool_t is_static;

    /* The core being used for multi-core systems */
    int core_ID;

    TaskPrio_t priority;

    /* Stack data */
    StackType_t *stack_start;
    StackType_t *stack_end;
    int stack_size;

    /* Task Name */
    char task_name[OS_MAX_TASK_NAME];

    /* IPC data */
    MessageQueue_t msg_queue;

    /* Mutex Data */
    int mutexes_held;
    int base_priority;

    /* List data */
    TCB_t *next_ptr;
    TCB_t *prev_ptr;

    /* Data for if the task is in a waiting list */
    TCB_t *waitlist_next_ptr;
    TCB_t *waitlist_prev_ptr;
    WaitList_t *waitlist;

    /* Tasks waiting on this task to die */
    WaitList_t *join_waitlist;

    /* Thread local storage pointers*/
    TLSPtr_t TLS_table[configNUM_THREAD_LOCAL_STORAGE_POINTERS];
    TLSPtrDeleteCallback_t TLS_delete_callback_table[configNUM_THREAD_LOCAL_STORAGE_POINTERS];

    /* State variables */
    OSTaskState_t task_state;

    /* Time to wake this task up if it has been delayed */
    TickType_t delay_wakeup_time;

    struct 	_reent xNewLib_reent;

    /* This list item is necessary for now until we are no longer reliant on FreeRTOS's
        Semaphore/timer/queue systems */
    ListItem_t	xEventListItem;

    /* A mutex for changing the state of this task */
    portMUX_TYPE task_state_mux;
};

/*******************************************************************************
* FUNCTION HEADERS
*******************************************************************************/

int OS_task_create(TaskFunc_t task_func, void *task_arg, const char * const task_name, 
            TaskPrio_t prio, int stack_size, int msg_queue_size, int core_ID, int *tid);

int OS_task_delete(Tid_t tid);

int OS_task_join(Tid_t tid, TickType_t timeout);

char * OS_task_get_name(Tid_t tid);

int OS_task_get_core_ID(Tid_t tid);

TaskPrio_t OS_task_get_priority(Tid_t tid);

void *OS_task_get_TLS_ptr(Tid_t tid, int index);

void OS_task_set_TLS_ptr(Tid_t tid, int index, void *value, TLSPtrDeleteCallback_t callback);

TCB_t * OS_task_get_tcb(Tid_t tid);

int OS_task_send_msg(Tid_t tid, TickType_t timeout, const void * const data);

int OS_task_receive_msg(TickType_t timeout, void ** data);

#endif /* OS_TASK_H */