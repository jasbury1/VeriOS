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

void OS_msg_queue_post()
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
