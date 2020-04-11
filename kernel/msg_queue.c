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
* STATIC FUNCTION DECLARATIONS
*******************************************************************************/

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
void OS_msg_queue_init(MessageQueue_t *msg_queue, int queue_size)
{
    msg_queue->num_messages = 0;
    msg_queue->max_messages = queue_size;
    msg_queue->head_ptr = NULL;
    msg_queue->tail_ptr = NULL;
    vPortCPUInitializeMutex(&msg_queue->mux);
}
