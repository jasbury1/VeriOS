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
#include "FreeRTOS_old.h"
#include "verios.h"
#include "task.h"
#include "schedule.h"
#include "msg_queue.h"
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
#define tskIDLE_STACK_SIZE	configIDLE_TASK_STACK_SIZE

/*******************************************************************************
* TASK CRITICAL STATE VARIABLES
*******************************************************************************/

/* Maps index representing a task ID (tid) to its corresponding tcb */
PRIVILEGED_DATA static volatile TCB_t ** OS_tid_table = NULL;

/* The current maximum capacity of the tid table */
PRIVILEGED_DATA static volatile int OS_tid_table_size = 0;

/* A counter of how many tasks have ever been created. Never decremented */
PRIVILEGED_DATA static volatile int OS_task_counter = 0;

/* Mutex for controling global task status events such as the tid table */
PRIVILEGED_DATA static portMUX_TYPE OS_task_mutex = portMUX_INITIALIZER_UNLOCKED;


/*******************************************************************************
* STATIC FUNCTION DECLARATIONS
*******************************************************************************/

static void _OS_task_init_tcb(TCB_t *tcb, const char * const task_name, TaskPrio_t prio, int stack_size, 
        OSBool_t is_static, const MemoryRegion_t * const mem_region, int core_ID);

static void _OS_task_init_stack(TCB_t *tcb, int stack_size, StackType_t *stack_alloc, 
        TaskFunc_t task_func, void *task_arg, TaskPrio_t prio);

static void _OS_task_delete_TLS(TCB_t *tcb);

static void _OS_task_delete_TCB(TCB_t *tcb);

static void _OS_task_init_tid(TCB_t *task_tcb);

/*******************************************************************************
* OS Task Create
*
*   task_func = The function representing a new task
*   task_arg = The argument passed to the task function
*   task_name = The name of the task for debugging purposes. Can be NULL
*   prio = The priority of the task. Less than OS_MAX_PRIORITIES
*   stack_size = The size of the stack measured in WORDS
*   msg_queu_size = The size of the IPC message queue. Use 0 or negative for no queue
*   core_ID = The ID of the core to place this task on
*   tcb_ptr = Pointer to space for which to reference a TCB_t* that will be used.
*             Can be null.
* 
* PURPOSE : 
*   
*   Create a new task by allocating necessary resources such as the TCB and stack
* 
* RETURN :
*   
*   Return an error code, or 0 (OS_NO_ERROR) if no error occured
*
* NOTES: 
*******************************************************************************/

int OS_task_create(TaskFunc_t task_func, void *task_arg, const char * const task_name, 
            TaskPrio_t prio, int stack_size, int msg_queue_size, int core_ID, Tid_t *task_tid)
{
    TCB_t *task_tcb;
    StackType_t *task_stack = NULL;
    int ret_val;
    
    /* Priority 0 is reserved for the Idle task */
    if(prio == (TaskPrio_t)0 && strcmp(task_name, OS_IDLE_NAME) != 0) {
        return OS_ERROR_RESERVED_PRIORITY;
    }

    /* Make sure the stack size is valid */
    if(stack_size <= 0) {
        return OS_ERROR_INVALID_STKSIZE;
    }

    /* Allocate Stack first so that TCB does not interact with stack memory */
	task_stack = ( StackType_t * ) pvPortMallocStackMem( ( ( ( size_t ) stack_size ) * sizeof( StackType_t ) ) );
    if(task_stack == NULL) {
        return OS_ERROR_STACK_ALLOC;
    }

    task_tcb = ( TCB_t * ) pvPortMallocTcbMem( sizeof( TCB_t ) );
    if(task_tcb == NULL){
        /* Allocating TCB failed. Free the stack and error out */
        vPortFree(task_stack);
        return OS_ERROR_TCB_ALLOC;
    }

    _OS_task_init_stack(task_tcb, stack_size, task_stack, task_func, task_arg, prio);
    _OS_task_init_tcb(task_tcb, task_name, prio, stack_size, OS_FALSE, NULL, core_ID);
    
    /* Get the task added to the tid table and set the task id */
    portENTER_CRITICAL(&OS_task_mutex);
    _OS_task_init_tid(task_tcb);
    portEXIT_CRITICAL(&OS_task_mutex);
    if(task_tid != NULL){ 
        *task_tid = task_tcb->tid;
    }
    
    /* Handle message queue */
    if(msg_queue_size > 0) {
        _OS_msg_queue_init(&(task_tcb->msg_queue), msg_queue_size);
    } 

    ret_val = OS_schedule_add_task(task_tcb);
    if(ret_val != OS_NO_ERROR) {
        return ret_val;
    }

    return OS_NO_ERROR;
}

/*******************************************************************************
* OS Task Delete
*
*   tcb: The tcb to de-allocate and remove from the scheduler.
* 
* PURPOSE :
*
*   Removes the tcb from schedule rotation and any other lists.
*   Will attempt to de-allocate all resources if possible. Otherwise they will
*   be left for IDLE to take care of.
* 
* RETURN : 
*
*   Return an error code, or 0 (OS_NO_ERROR) if no error occured
*
* NOTES: 
*******************************************************************************/

int OS_task_delete(Tid_t tid)
{
    int ret_val;
    TCB_t *tcb;

    /* Determine which tcb is to be deleted */
    tcb = OS_task_get_tcb(tid);
    if(tcb == NULL) {
        return OS_ERROR_INVALID_TID;
    }

    /* Cannot delete the IDLE task */
    if(tcb->priority == OS_IDLE_PRIORITY){
        return OS_ERROR_IDLE_DELETE;
    }

    /* Remove this task from the scheduler */
    ret_val = OS_schedule_remove_task(tcb);
    
    /* Only get here if the task was deleted from another context */
    if(ret_val != OS_NO_ERROR) {
        return ret_val;
    }
    assert(tcb->task_state == OS_TASK_STATE_READY_TO_DELETE);
    _OS_task_delete_TLS(tcb);
    _OS_task_delete_TCB(tcb);

    return OS_NO_ERROR;
}

/*******************************************************************************
* OS Task Join
*
* PURPOSE :
*
* RETURN : 
*
*
* NOTES: 
*******************************************************************************/

int OS_task_join(Tid_t tid, TickType_t timeout)
{
    TCB_t *tcb = OS_task_get_tcb(tid);
    if(tcb == NULL) {
        return OS_ERROR_INVALID_TID;
    }

    int ret_val;
    TCB_t *waiter = OS_schedule_get_current_tcb();
    if(tcb->join_waitlist == NULL) {
        tcb->join_waitlist = malloc(sizeof(WaitList_t));
        tcb->join_waitlist->head_ptr = NULL;
        tcb->join_waitlist->tail_ptr = NULL;
        tcb->join_waitlist->num_tasks = 0;
    }

    ret_val = OS_schedule_join_list_insert(waiter, tcb, timeout);
    return ret_val;
}

/*******************************************************************************
* OS Task Get Name
*
*   tcb: A pointer to the desired TCB
* 
* PURPOSE : 
*
*   Get a task's name
* 
* RETURN : 
*
*   A pointer to the first character of the task name
*
* NOTES: 
*******************************************************************************/

char * OS_task_get_name(Tid_t tid)
{
    TCB_t *tcb = OS_task_get_tcb(tid);
    assert(tcb);
    return &(tcb->task_name[0]);
}

/*******************************************************************************
* OS Task Get Core ID
*
*   tcb: A pointer to the desired TCB 
* 
* PURPOSE : 
*
*   Get the core ID that a task was originally assigned to
* 
* RETURN :
*
*   The core ID or CORE_NO_AFFINITY if the task was not assigned to a specific core
*
* NOTES: 
*******************************************************************************/

int OS_task_get_core_ID(Tid_t tid)
{
    TCB_t *tcb = OS_task_get_tcb(tid);
    assert(tcb);
    return tcb->core_ID;
}

/*******************************************************************************
* OS Task Get Priority
*
*   tcb: A pointer to the desired TCB 
* 
* PURPOSE : 
*
*   Get the priority of the desired task
* 
* RETURN :
*
*   The priority assigned to the task
*
* NOTES: 
*******************************************************************************/

TaskPrio_t OS_task_get_priority(Tid_t tid)
{
    TCB_t *tcb = OS_task_get_tcb(tid);
    assert(tcb);
    return tcb->priority;
}

/*******************************************************************************
* OS Task Get Priority
*
*   tcb: A pointer to the desired TCB 
* 
* PURPOSE : 
*
*   Get the priority of the desired task
* 
* RETURN :
*
*   The priority assigned to the task
*
* NOTES: 
*******************************************************************************/

void *OS_task_get_TLS_ptr(Tid_t tid, int index)
{
    TCB_t *tcb = OS_task_get_tcb(tid);
    assert(tcb);
    if(index >= configNUM_THREAD_LOCAL_STORAGE_POINTERS) {
        return NULL;
    } 
    return tcb->TLS_table[index];
}

/*******************************************************************************
* OS Schedule Set Task TLS Ptr
*
*   tcb = The pointer to the task whose pointer value is getting updated
*   index = The index into the TLS table to use 
*   value = The pointer that gets placed inside the TLS table
*   callback = The function used to delete the TLS pointer upon task deletion
* 
* PURPOSE :
*
*   Sets the value of a thread local storage pointer. Must include a delete
*   callback so that the OS knows how to free the data upon task deletion
* 
* RETURN : 
*
* NOTES:
*******************************************************************************/

/* TODO: make the indexing smarter one day... */
void OS_task_set_TLS_ptr(Tid_t tid, int index, 
        void *value, TLSPtrDeleteCallback_t callback)
{
    TCB_t *tcb = OS_task_get_tcb(tid);
    assert(tcb);

    if(index < configNUM_THREAD_LOCAL_STORAGE_POINTERS) {
        portENTER_CRITICAL(&(tcb->task_state_mux));
        tcb->TLS_table[index] = value;
        tcb->TLS_delete_callback_table[index] = callback;
        portEXIT_CRITICAL(&(tcb->task_state_mux));
    }
}

/*******************************************************************************
* OS Task Get Priority
*
*   tcb: A pointer to the desired TCB 
* 
* PURPOSE : 
*
*   Get the priority of the desired task
* 
* RETURN :
*
*   The priority assigned to the task
*
* NOTES: 
*******************************************************************************/

int OS_task_send_msg(Tid_t tid, TickType_t timeout, const void * const data)
{
    TCB_t *tcb = OS_task_get_tcb(tid);
    if(tcb == NULL){
        return OS_ERROR_INVALID_TID;
    }

    int ret_val;
    if(tcb->msg_queue.max_messages <= 0) {
        return OS_ERROR_NO_TASK_QUEUE;
    }

    ret_val = OS_msg_queue_post(&(tcb->msg_queue), timeout, data);
    return ret_val;
}

/*******************************************************************************
* OS Task Get Priority
*
*   tcb: A pointer to the desired TCB 
* 
* PURPOSE : 
*
*   Get the priority of the desired task
* 
* RETURN :
*
*   The priority assigned to the task
*
* NOTES: 
*******************************************************************************/

int OS_task_receive_msg(TickType_t timeout, void ** data)
{
    TCB_t *cur_tcb;
    int ret_val;

    cur_tcb = OS_schedule_get_current_tcb();
    assert(cur_tcb);

    ret_val = OS_msg_queue_pend(&(cur_tcb->msg_queue), timeout, data);
    return ret_val;
}

/*******************************************************************************
* OS Task Get Priority
*
*   tcb: A pointer to the desired TCB 
* 
* PURPOSE : 
*
*   Get the priority of the desired task
* 
* RETURN :
*
*   The priority assigned to the task
*
* NOTES: 
*******************************************************************************/

TCB_t * OS_task_get_tcb(Tid_t tid)
{
    if(tid >= OS_task_counter || tid < OS_CURRENT_TASK) {
        return NULL;
    }
    if(tid == OS_CURRENT_TASK){
       return OS_schedule_get_current_tcb(); 
    }
    return OS_tid_table[tid];
}

/*******************************************************************************
* STATIC FUNCTION DEFINITIONS
*******************************************************************************/

/**
 * Delete Thread-Local-Storage pointers if any exist for the given task
 */
static void _OS_task_delete_TLS(TCB_t *tcb)
{
    int i;
    for(i = 0; i < configNUM_THREAD_LOCAL_STORAGE_POINTERS; ++i){
        if (tcb->TLS_delete_callback_table[i] != NULL) {
			tcb->TLS_delete_callback_table[i](i, tcb->TLS_table[i]);
		}
    }
}

/**
 * Delete and free the TCB itself
 */
static void _OS_task_delete_TCB(TCB_t *tcb)
{
    _reclaim_reent( &( tcb->xNewLib_reent ) );

    vPortReleaseTaskMPUSettings( &(tcb->MPU_settings) );
    /* TODO: Clean up anything else? Message Queues? */

    /* If the task is dynamically allocated */
    if(tcb->is_static == OS_FALSE) {
        /* Free the stack and TCB itself */
        vPortFreeAligned(tcb->stack_start);
		free(tcb);
    }
    if(tcb->join_waitlist != NULL){
        free(tcb->join_waitlist);
    }
    /* Stack is statically allocated */
    else {
        /* TODO: just the stack or tcb static? or are both? Handle these cases */
    }
}

static void _OS_task_init_tcb(TCB_t *tcb, const char * const task_name, TaskPrio_t prio, int stack_size, 
        OSBool_t is_static, const MemoryRegion_t * const mem_region, int core_ID)
{
    int i;

    tcb->is_static = is_static;
    tcb->priority = prio;
    tcb->base_priority = prio;
    tcb->core_ID = core_ID;
    tcb->mutexes_held = 0;
    tcb->delay_wakeup_time = 0;
    
    /* Initialize list parameters to null for now */
    tcb->next_ptr = NULL;
    tcb->prev_ptr = NULL;

    /* Initialize waitlist/blocking data */
    tcb->is_blocked = OS_FALSE;
    tcb->block_record.timeout_remaining = 0;
    tcb->block_record.waitlist = NULL;
    tcb->block_record.waitlist_next_ptr = NULL;
    tcb->block_record.waitlist_prev_ptr = NULL;

    /* Initialize join waitlist to null for now */
    tcb->join_waitlist = NULL;

    /* Take care of event list systems as we are reliant on FreeRTOS for
        Semaphores/queues/timers */
    vListInitialiseItem( &(tcb->xEventListItem ) );
    listSET_LIST_ITEM_VALUE( &(tcb->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) prio );
	listSET_LIST_ITEM_OWNER( &(tcb->xEventListItem ), tcb);

    /* Copy the task name into the buffer */
    for( i = 0; i < OS_MAX_TASK_NAME; i++) {
		tcb->task_name[i] = task_name[i];
		if( task_name[i] == 0x00 ) {
			break;
		}
	}

    /* Initialize the TLS table to null */
    for( i = 0; i < configNUM_THREAD_LOCAL_STORAGE_POINTERS; ++i) {
        tcb->TLS_table[i] = NULL;
        tcb->TLS_delete_callback_table[i] = NULL;
    }

    /* Initialize the tasks internal state mtex */
    vPortCPUInitializeMutex(&tcb->task_state_mux);

    esp_reent_init(&tcb->xNewLib_reent);

	vPortStoreTaskMPUSettings( &(tcb->MPU_settings), mem_region, tcb->stack_start, stack_size );
}

static void _OS_task_init_stack(TCB_t *tcb, int stack_size, StackType_t *stack_alloc, 
        TaskFunc_t task_func, void *task_arg, TaskPrio_t prio)
{
    StackType_t *stack_top;
    OSBool_t run_privileged;

    #if( portUSING_MPU_WRAPPERS == 1) 
    if((prio & OS_PRIVILEGE_BIT) != 0U) {
        run_privileged = OS_TRUE;
    } 
    else {
        run_privileged = OS_FALSE;
    }
    prio &= ~OS_PRIVILEGE_BIT;
    #endif /* portUSING_MPU_WRAPPERS */

    tcb->stack_start = stack_alloc;
    ( void ) memset( tcb->stack_start, ( int ) OS_STACK_FILL_BYTE, ( size_t ) stack_size * sizeof( StackType_t ) );
    
    stack_top = tcb->stack_start + (stack_size - (uint32_t) 1);
    stack_top = (StackType_t *)(((portPOINTER_SIZE_TYPE) stack_top) & ( ~((portPOINTER_SIZE_TYPE) portBYTE_ALIGNMENT_MASK)));
    tcb->stack_end = stack_top;
    tcb->stack_top = pxPortInitialiseStack(stack_top, task_func, task_arg, run_privileged);
}

static void _OS_task_init_tid(TCB_t *task_tcb)
{
    if(OS_tid_table == NULL) {
        OS_tid_table = calloc(OS_TID_TABLE_INITIAL_SIZE, sizeof(TCB_t *));
        OS_tid_table_size = OS_TID_TABLE_INITIAL_SIZE;
    }
    else if(OS_tid_table_size == OS_task_counter){
        OS_tid_table_size *= 2;
        realloc(OS_tid_table, OS_tid_table_size * sizeof(TCB_t *));
    }
    task_tcb->tid = OS_task_counter;
    OS_tid_table[task_tcb->tid] = task_tcb;
    ++OS_task_counter;
}

