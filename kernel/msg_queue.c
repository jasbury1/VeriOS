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
#include "verios_util.h"
#include "StackMacros.h"
#include "portmacro.h"
#include "portmacro_priv.h"
#include "semphr.h"

/*******************************************************************************
* SCHEDULER CRITICAL STATE VARIABLES
*******************************************************************************/

/* A list of messages for use/reuse. Works similar to 'slab allocation' */
PRIVILEGED_DATA static volatile MessagePool_t OS_msg_pool = {0, NULL, NULL};

/* A counter of all messages that have been allocated and exist in various places */
PRIVILEGED_DATA static volatile int OS_msg_count = 0;

PRIVILEGED_DATA static portMUX_TYPE OS_message_mutex = portMUX_INITIALIZER_UNLOCKED;

/*******************************************************************************
* STATIC FUNCTION DECLARATIONS
*******************************************************************************/

static Message_t * _OS_msg_pool_retrieve(void);

static void _OS_msg_pool_insert(Message_t *msg);

static void _OS_msg_queue_insert(MessageQueue_t *msg_queue, Message_t *msg);

static Message_t * _OS_msg_queue_pop(MessageQueue_t *msg_queue);

/*******************************************************************************
* Message Queue Create (API FUNCTION)
*
*   queue_ptr = A reference to the pointer where the queue will be allocated
*   queue_size = The maximum number of messages the queue should hold 
* 
* PURPOSE :
*
*   This is the API call for creating a new queue for messages. One or more tasks
*   can then use this queue for IPC message reading/writing.
* 
* RETURN :
*   
*   Returns an error code or 0 (OS_NO_ERROR) if no error occurred
*
* NOTES: 
*******************************************************************************/

int OS_msg_queue_create(MsgQueue_t * queue_ptr, int queue_size)
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
    
    *queue_ptr = (void *)msg_queue;

    return OS_NO_ERROR;
}

/*******************************************************************************
* Message Queue Delete (API FUNCTION)
*
*   queue = The reference to the message queue to delete
* 
* PURPOSE : 
*  
*   Destroy a message queue. Free up all of its waiting tasks and return all of
*   the contained messages back into the message pool
* 
* RETURN :
*   
*   Returns an error code or 0 (OS_NO_ERROR) if no error occurred
*
* NOTES: 
*******************************************************************************/

int OS_msg_queue_delete(MsgQueue_t queue)
{
    MessageQueue_t *msg_queue = (MessageQueue_t *)queue;
    Message_t *retrieved_message;

    if(msg_queue == NULL) {
        return OS_ERROR_INVALID_QUEUE;
    }

    portENTER_CRITICAL(&(msg_queue->mux));
    OS_schedule_waitlist_empty(&(msg_queue->reveive_waiters));
    OS_schedule_waitlist_empty(&(msg_queue->send_waiters));

    while(msg_queue->num_messages != 0) {
        retrieved_message = _OS_msg_queue_pop(msg_queue);

        /* We are done with this message. Add to the pool for future re-use */
        portENTER_CRITICAL(&OS_message_mutex);
        _OS_msg_pool_insert(retrieved_message);
        portEXIT_CRITICAL(&OS_message_mutex);
    }
    portEXIT_CRITICAL(&(msg_queue->mux));
    free(msg_queue);
    msg_queue = NULL;
    return OS_NO_ERROR;
}

/*******************************************************************************
* Message Queue Send (API FUNCTION)
*
*   queue = The reference to the message queue to send the message to
*   timeout = Max amount of time to wait if the queue is full
*   data = A pointer to the data the message is carrying. Usually allocated
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
*   Return an error message or 0 (OS_NO_ERROR) if no error occurred   
*
* NOTES: 
*
*   Use OS_NO_TIMEOUT as the timeout if this call should not have a timeout
*******************************************************************************/

int OS_msg_queue_send(MsgQueue_t queue, TickType_t timeout, const void * const data)
{
    MessageQueue_t *msg_queue = (MessageQueue_t *)queue;
    Message_t *new_message = NULL;
    TCB_t *sender = OS_schedule_get_current_tcb();
    TCB_t *waiting_receiver = NULL;

    if(msg_queue == NULL) {
        return OS_ERROR_INVALID_QUEUE;
    }

    while (OS_TRUE) {
        portENTER_CRITICAL(&(msg_queue->mux));
        
        /* Add the message if there is room on the queue */
        if (msg_queue->num_messages < msg_queue->max_messages)
        {
            
            portENTER_CRITICAL(&OS_message_mutex);
            new_message = _OS_msg_pool_retrieve();
            portEXIT_CRITICAL(&OS_message_mutex);
            
            if (new_message == NULL)
            {
                portEXIT_CRITICAL(&(msg_queue->mux));
                sender->is_blocked = OS_FALSE;
                return OS_ERROR_MSG_POOL_RETR;
            }

            new_message->sender = sender;
            new_message->contents = data;
            new_message->next_ptr = NULL;

            _OS_msg_queue_insert(msg_queue, new_message);

            /* If tasks are waiting on this message queue, wake them up */
            if(msg_queue->reveive_waiters.num_tasks != 0){
                waiting_receiver = _OS_waitlist_pop_head(&(msg_queue->reveive_waiters));
            }

            portEXIT_CRITICAL(&(msg_queue->mux));

            if(waiting_receiver != NULL){
                /* Schedule the task that was waiting on a new message */
                OS_schedule_resume_task(waiting_receiver);
            }
            sender->is_blocked = OS_FALSE;
            return OS_NO_ERROR;
        }

        /* We were unable to add the message */

        /* The queue was destroyed */
        if(msg_queue == NULL) {
            portEXIT_CRITICAL(&(msg_queue->mux));
            sender->is_blocked = OS_FALSE;
            return OS_ERROR_RESOURCE_DESTROYED;
        }

        /* The timeout already went off, but we were unable to find queue room */
        if(sender->is_blocked == OS_TRUE) {
            portEXIT_CRITICAL(&(msg_queue->mux));
            sender->is_blocked = OS_FALSE;
            return OS_ERROR_QUEUE_FULL;
        }

        /* Add the task to a waitlist so that it can be woken up if theres room in the queue */
        _OS_waitlist_append(sender, &(msg_queue->send_waiters));
        
        portEXIT_CRITICAL(&(msg_queue->mux));

        sender->is_blocked = OS_TRUE;
        OS_schedule_delay_task(sender, timeout);

    }
    sender->is_blocked = OS_FALSE;
    return OS_NO_ERROR;
}

/*******************************************************************************
* Message Queue Receive (API FUNCTION)
*
*   _queue = The referemce to the message queue to read from
*   timeout = The amount of time to wait for a message if the queue is empty
* 
* PURPOSE : 
*
*   The main API call for reading a message from a message queue. This function
*   will block if the queue is empty and the task will be placed in a waiting list.
*   If a timeout is specified and the task times out before a message is added in the
*   queue, the task will be removed from the waitlist and rescheduled without reading
*   any message.  
*   The data pointer is filled with the data or set to NULL if no data could be
*   retrieved before the timeout expired
* 
* RETURN :
*
*   Returns an error code or 0 (OS_NO_ERROR) if no error occurred
*
* NOTES: 
*
*   Use OS_NO_TIMEOUT as the timeout if this call should not time out
*******************************************************************************/

int OS_msg_queue_receive(MsgQueue_t queue, TickType_t timeout, void ** data)
{
    MessageQueue_t * msg_queue = (MessageQueue_t *)queue;
    Message_t *retrieved_message = NULL;
    TCB_t *receiver = OS_schedule_get_current_tcb();
    TCB_t *waiting_sender = NULL;

    void *msg_contents;

    if(msg_queue == NULL) {
        return OS_ERROR_INVALID_QUEUE;
    }

    while(OS_TRUE) {
        portENTER_CRITICAL(&(msg_queue->mux));

        /* There is a message on the queue that we can retrieve */
        if(msg_queue->num_messages > 0) {
            retrieved_message = _OS_msg_queue_pop(msg_queue);
            msg_contents = retrieved_message->contents;

            /* We are done with this message. Add to the pool for future re-use */
            portENTER_CRITICAL(&OS_message_mutex);
            _OS_msg_pool_insert(retrieved_message);
            portEXIT_CRITICAL(&OS_message_mutex);

            /* If tasks are waiting on this message queue, wake them up */
            if(msg_queue->send_waiters.num_tasks != 0){
                waiting_sender = _OS_waitlist_pop_head(&(msg_queue->send_waiters));
            }

            portEXIT_CRITICAL(&(msg_queue->mux));

            if(waiting_sender != NULL){
                /* Schedule the task that was waiting on a new message */
                OS_schedule_resume_task(waiting_sender);
            }
            *data = msg_contents;
            receiver->is_blocked = OS_FALSE;
            return OS_NO_ERROR;
        }

        /* We were unable to read because no messages were sent */

        /* The queue was destroyed */
        if(msg_queue == NULL){
            portEXIT_CRITICAL(&(msg_queue->mux));
            *data = NULL;
            receiver->is_blocked = OS_FALSE;
            return OS_ERROR_RESOURCE_DESTROYED;
        }

        /* The timeout expired */
        if(receiver->is_blocked == OS_TRUE) {
            portEXIT_CRITICAL(&(msg_queue->mux));
            *data = NULL;
            receiver->is_blocked = OS_FALSE;
            return OS_ERROR_TIMER_EXPIRED;
        }

        /* Add the task to a waitlist so that it can be woken up if theres room in the queue */
        _OS_waitlist_append(receiver, &(msg_queue->reveive_waiters));
        
        portEXIT_CRITICAL(&(msg_queue->mux));

        receiver->is_blocked = OS_TRUE;
        OS_schedule_delay_task(receiver, timeout);
    }
    receiver->is_blocked = OS_FALSE;
    return OS_NO_ERROR;
}

/*******************************************************************************
* Message Queue Try Send (API FUNCTION)
*
*   queue = A reference to the message queue to send the message to
*   data = A pointer to the data the message is carrying. Usually allocated 
* 
* PURPOSE : 
*
*   Another API call for writing a message to a message queue. This function
*   will NOT block if the queue is full and will instead return immediately.
* 
* RETURN :
*
*   Returns an OS_NO_ERROR if the message was sent successfully. Returns -1 if
*   The queue was full and the send did not complete. Returns an error code
*   if an error occured.
*
* NOTES:
*
*   This function is non-blocking and will not wait to send a message if the
*   queue is full. Use OS_msg_queue_send for the blocking equivalent
*******************************************************************************/

int OS_msg_queue_try_send(MsgQueue_t queue, const void * const data)
{
    MessageQueue_t * msg_queue = (MessageQueue_t *)queue;
    Message_t *new_message = NULL;
    TCB_t *sender = OS_schedule_get_current_tcb();
    TCB_t *waiting_receiver = NULL;

    if(msg_queue == NULL) {
        return OS_ERROR_INVALID_QUEUE;
    }

    portENTER_CRITICAL(&(msg_queue->mux));
    
    /* Add the message if there is room on the queue */
    if (msg_queue->num_messages < msg_queue->max_messages)
    {
        
        portENTER_CRITICAL(&OS_message_mutex);
        new_message = _OS_msg_pool_retrieve();
        portEXIT_CRITICAL(&OS_message_mutex);
        
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
            waiting_receiver = _OS_waitlist_pop_head(&(msg_queue->reveive_waiters));
        }

        portEXIT_CRITICAL(&(msg_queue->mux));

        if(waiting_receiver != NULL){
            /* Schedule the task that was waiting on a new message */
            OS_schedule_resume_task(waiting_receiver);
        }

        return OS_NO_ERROR;
    }

    portEXIT_CRITICAL(&(msg_queue->mux));
    return -1;
}

/*******************************************************************************
* Message Queue Try Reveive (API FUNCTION)
*
*   queue = A reference to the message queue to read from
*   data = A double pointer that will be filled with the data retrieved
* 
* PURPOSE : 
*
*   Another API call for reading a message from a message queue. This function
*   will NOT block if the queue is empty and will instead return immediately.
*   The data pointer is filled with the data or set to NULL if no data could be
*   retrieved  
* 
* RETURN :
*
*   Returns a void pointer to the data read from the message. Returns a NULL ptr
*   if the queue was empty
*
* NOTES:
*
*   This function is non-blocking and will not wait to read a message if the
*   queue is empty. Use OS_msg_queue_receive for the blocking equivalent
*******************************************************************************/

int OS_msg_queue_try_receive(MsgQueue_t queue, void ** data)
{
    MessageQueue_t *msg_queue = (MessageQueue_t *)queue;
    Message_t *retrieved_message = NULL;
    TCB_t *waiting_sender = NULL;

    void *msg_contents;

    if(msg_queue == NULL) {
        return OS_ERROR_INVALID_QUEUE;
    }

    portENTER_CRITICAL(&(msg_queue->mux));

    /* There is a message on the queue that we can retrieve */
    if(msg_queue->num_messages > 0) {
        retrieved_message = _OS_msg_queue_pop(msg_queue);
        msg_contents = retrieved_message->contents;

        /* We are done with this message. Add to the pool for future re-use */
        portENTER_CRITICAL(&OS_message_mutex);
        _OS_msg_pool_insert(retrieved_message);
        portEXIT_CRITICAL(&OS_message_mutex);

        /* If tasks are waiting on this message queue, wake them up */
        if(msg_queue->send_waiters.num_tasks != 0){
            waiting_sender = _OS_waitlist_pop_head(&(msg_queue->send_waiters));
        }

        portEXIT_CRITICAL(&(msg_queue->mux));

        if(waiting_sender != NULL){
            /* Schedule the task that was waiting on a new message */
            OS_schedule_resume_task(waiting_sender);
        }

        *data = msg_contents;
        return OS_NO_ERROR;
    }
    portEXIT_CRITICAL(&(msg_queue->mux));
    *data = NULL;
    return OS_ERROR_QUEUE_EMPTY;
}

/*******************************************************************************
* OS Message Queue Init
*
*   msg_queue = A reference to the message queue to be initialized
*   queue_size = The maximum capacity of the message queue
* 
* PURPOSE : 
*   
*   Initializes the data members in a message queue
* 
* RETURN :
*   
*
* NOTES: 
*******************************************************************************/

void _OS_msg_queue_init(MessageQueue_t *msg_queue, int queue_size)
{
    msg_queue->num_messages = 0;
    msg_queue->max_messages = queue_size;
    msg_queue->head_ptr = NULL;
    msg_queue->tail_ptr = NULL;

    _OS_list_header_init(&(msg_queue->reveive_waiters));
    _OS_list_header_init(&(msg_queue->send_waiters));

    vPortCPUInitializeMutex(&msg_queue->mux);

}

/*******************************************************************************
* STATIC FUNCTION DEFINITIONS
*******************************************************************************/

static void _OS_msg_pool_insert(Message_t *msg)
{
    /* The message pool is empty */
    if(OS_msg_pool.num_messages == 0) {
        OS_msg_pool.head_ptr = msg;
        OS_msg_pool.tail_ptr = msg;
    }
    /* The message pool isn't empty. Add to the tail */
    else {
        OS_msg_pool.tail_ptr->next_ptr = msg;
        OS_msg_pool.tail_ptr = msg;
    }
    OS_msg_pool.tail_ptr->next_ptr = NULL;
    OS_msg_pool.num_messages++;
    assert(OS_msg_pool.head_ptr);
}

static Message_t* _OS_msg_pool_retrieve(void)
{
    Message_t *msg;
    int i;
    int size;

    /* Our pool is empty so we have to allocate another slab of messages */
    if(OS_msg_pool.num_messages == 0){
        /* First allocation is a fixed size. Later allocations double the size */
        //size = OS_msg_count == 0 ? OS_MSG_POOL_INITIAL_SIZE : OS_msg_count;
        size = 8;

        OS_msg_pool.head_ptr = pvPortMalloc(sizeof(Message_t) * size);
        assert(OS_msg_pool.head_ptr);

        /* Connect the newly allocated messages as a pool list */
        for(i = 0; i < size - 1; ++i) {
            (&(OS_msg_pool.head_ptr)[i])->next_ptr = &(OS_msg_pool.head_ptr)[i + 1];
        }
        (OS_msg_pool.head_ptr)[size - 1].next_ptr = NULL;
        OS_msg_pool.tail_ptr = &((OS_msg_pool.head_ptr)[size - 1]);
        OS_msg_pool.num_messages = size;

        OS_msg_count += size;
    }

    OS_msg_pool.num_messages--;
    /* Remove the head of the list to return as the message for use */
    assert(OS_msg_pool.head_ptr);
    msg = OS_msg_pool.head_ptr;

    OS_msg_pool.head_ptr = OS_msg_pool.head_ptr->next_ptr;
    if(OS_msg_pool.head_ptr == NULL){
        assert(OS_msg_pool.num_messages == 0);
    }
    msg->next_ptr = NULL;
    return msg;

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
    msg->next_ptr = NULL;
    msg_queue->num_messages++;
}

static Message_t * _OS_msg_queue_pop(MessageQueue_t *msg_queue)
{
    Message_t * msg; 
    assert(msg_queue->num_messages > 0);

    msg = msg_queue->head_ptr;
    msg_queue->head_ptr = msg_queue->head_ptr->next_ptr;
    msg->next_ptr = NULL;
    
    /* If we only had one message in the queue, update the tail */
    if(msg_queue->head_ptr == NULL) {
        msg_queue->tail_ptr = NULL;
    }
    msg_queue->num_messages--;

    return msg;
}


