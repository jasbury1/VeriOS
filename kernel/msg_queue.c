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

/*******************************************************************************
* SCHEDULER CRITICAL STATE VARIABLES
*******************************************************************************/

PRIVILEGED_DATA static MessagePool_t OS_msg_pool = {0, NULL, NULL};

/* A counter of all messages that have been allocated and exist in various places */
PRIVILEGED_DATA static int OS_msg_count = 0;

/*******************************************************************************
* STATIC FUNCTION DECLARATIONS
*******************************************************************************/

static void _OS_msg_queue_waitlist_init(WaitList_t *list);

static Message_t* _OS_msg_pool_retrieve(void);

static void _OS_msg_queue_insert(MessageQueue_t *msg_queue, Message_t *msg);

/*******************************************************************************
* OS Message Queue Init
*
*   msg_queue = 
*   queue_size = 
* 
* PURPOSE : 
*   
* 
* RETURN :
*   
*
* NOTES: 
*******************************************************************************/

/* TODO: Maybe this becomes static at some point and we have another method
    for initializing task IPC data */
void _OS_msg_queue_init(MessageQueue_t *msg_queue, int queue_size)
{
    msg_queue->num_messages = 0;
    msg_queue->max_messages = queue_size;
    msg_queue->head_ptr = NULL;
    msg_queue->tail_ptr = NULL;

    _OS_msg_queue_waitlist_init(&(msg_queue->reveive_waiters));
    _OS_msg_queue_waitlist_init(&(msg_queue->send_waiters));

    vPortCPUInitializeMutex(&msg_queue->mux);
}

/*******************************************************************************
* OS Message Queue Init
*
*   queue_ptr = Double pointer to the queue itself
*   queue_size = The maximum number of messages the queue should hold 
* 
* PURPOSE :
*
*   This is the API call for creating a new queue for messages. One or more tasks
*   can then use this queue for IPC message reading/writing.
* 
* RETURN :
*   
*
* NOTES: 
*******************************************************************************/

int OS_msg_queue_create(void ** queue_ptr, int queue_size)
{
    MessageQueue_t *msg_queue = NULL;

    if(queue_size <= 0 || queue_size > OS_MAX_MSG_QUEUE_SIZE) {
        return OS_ERROR_INVALID_QUEUE_SIZE;
    }

    if(queue_ptr == NULL){
        return OS_ERROR_QUEUE_NULL_PTR;
    }

    /* Allocate the queue */
    msg_queue = malloc(sizeof(MessageQueue_t));
    if(msg_queue == NULL) {
        return OS_ERROR_QUEUE_ALLOC;
    }

    _OS_msg_queue_init(msg_queue, queue_size);
    
    *queue_ptr = msg_queue;

    return OS_NO_ERROR;
}

/*******************************************************************************
* OS Message Queue Init
*
*   msg_queue = 
*   queue_size = 
* 
* PURPOSE : 
*   
* 
* RETURN :
*   
*
* NOTES: 
*******************************************************************************/

int OS_msg_queue_delete(MessageQueue_t *msg_queue)
{
    configASSERT(OS_FALSE);
    return OS_NO_ERROR;
}

/*******************************************************************************
* OS Message Queue Post
*
*   msg_queue = 
*   timeout =
*   data = 
* 
* PURPOSE : 
*
*   The main API call for sending a message to a message queue. This function
*   will block if the queue is full and the task will be placed in a waiting list.
*   If a timeout is specified and the task times out before room is made in the
*   queue, the task will be removed from the waitlist and rescheduled without sending
*   the message.
* 
* RETURN :
*   
*
* NOTES: 
*
*   If the timeout is equal to the max delay time, the task will be suspended
*   and assumed to have no timeout
*   If the sending tasks block, it is up to the next task that reads from the
*   queue to unblock the next waiting task
*******************************************************************************/

int OS_msg_queue_post(MessageQueue_t *msg_queue, TickType_t timeout, const void * const data)
{
    Message_t *new_message = NULL;
    TCB_t *sender = OS_schedule_get_current_tcb();
    TCB_t *waiting_receiver = NULL;
    OSBool_t timeout_set = OS_FALSE;

    configASSERT(sender->task_state == OS_TASK_STATE_RUNNING || sender->task_state == OS_TASK_STATE_READY);

    while (OS_TRUE) {
        portENTER_CRITICAL(&(msg_queue->mux));
        
        /* Add the message if there is room on the queue */
        if (msg_queue->num_messages < msg_queue->max_messages)
        {
            new_message = _OS_msg_pool_retrieve();

            if (new_message == NULL)
            {
                return OS_ERROR_MSG_POOL_RETR;
            }

            new_message->sender = sender;
            new_message->contents = data;
            new_message->next_ptr = NULL;

            _OS_msg_queue_insert(msg_queue, new_message);

            /* If tasks are waiting on this message queue, wake them up */
            if(msg_queue->reveive_waiters.num_tasks != 0){
                waiting_receiver = OS_waitlist_pop_head(&(msg_queue->reveive_waiters));
            }

            portEXIT_CRITICAL(&(msg_queue->mux));

            if(waiting_receiver != NULL){
                /* Schedule the task that was waiting on a new message */
                OS_schedule_resume_task(waiting_receiver);
            }

            return OS_NO_ERROR;
        }

        /* We were unable to add the message due to queue being full */

        /* The timeout already went off, but we were unable to find queue room */
        if(timeout_set == OS_TRUE) {
            return OS_ERROR_QUEUE_FULL;
        }

        /* Add the task to a waitlist so that it can be woken up if theres room in the queue */
        OS_waitlist_insert_task(sender, &(msg_queue->send_waiters));
        
        portEXIT_CRITICAL(&(msg_queue->mux));

        /* If the task is in the ready list, it must be unscheduled and blocked */
        if(sender->task_state != OS_TASK_STATE_DELAYED && sender->task_state != OS_TASK_STATE_SUSPENDED) {
            if(timeout == portMAX_DELAY){
                OS_schedule_suspend_task(sender);
            }
            else {
                timeout_set = OS_TRUE;
                OS_schedule_delay_task(sender, timeout);
            }
        }
        /* Yielding didn't happen implicitely when suspending or delaying */
        else {
            portYIELD_WITHIN_API();
        }
    }
    return OS_NO_ERROR;

}

/*******************************************************************************
* OS Message Queue Init
*
*   msg_queue = 
*   queue_size = 
* 
* PURPOSE : 
*   
* 
* RETURN :
*   
*
* NOTES: 
*******************************************************************************/

void OS_msg_queue_pend()
{

}

/*******************************************************************************
* OS Message Queue Init
*
*   msg_queue = 
*   queue_size = 
* 
* PURPOSE : 
*   
* 
* RETURN :
*   
*
* NOTES: 
*******************************************************************************/

void OS_msg_queue_try_pend()
{

}

/*******************************************************************************
* OS Message Queue Init
*
*   msg_queue = 
*   queue_size = 
* 
* PURPOSE : 
*   
* 
* RETURN :
*   
*
* NOTES: 
*******************************************************************************/

void OS_msg_queue_try_post()
{

}

/*******************************************************************************
* STATIC FUNCTION DEFINITIONS
*******************************************************************************/

static void _OS_msg_queue_waitlist_init(WaitList_t *list)
{
    list->head_ptr = NULL;
    list->tail_ptr = NULL;
    list->num_tasks = 0;
}

static Message_t* _OS_msg_pool_retrieve(void)
{
    return NULL;
}

static void _OS_msg_queue_insert(MessageQueue_t *msg_queue, Message_t *msg)
{
    /* If this is the first message in the queue */
    if (msg_queue->head_ptr == NULL)
    {
        msg_queue->head_ptr = msg;
        msg_queue->tail_ptr = msg;
    }
    /* Add to the tail */
    else
    {
        msg_queue->tail_ptr->next_ptr = msg;
        msg_queue->tail_ptr = msg;
    }
    msg_queue->num_messages++;
}


