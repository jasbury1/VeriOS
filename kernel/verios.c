/* Standard includes. */
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"

#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "esp_newlib.h"
#include "esp_compiler.h"

/* FreeRTOS includes. */
#include "FreeRTOS_old.h"
#include "verios.h"
#include "task.h"

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

void OS_waitlist_insert_task(TCB_t *tcb, WaitList_t *waitlist)
{
    TCB_t *tcb1;
    TCB_t *tcb2;

    /* Update the task counter for this delayed list */
    waitlist->num_tasks++;
    tcb->waitlist = waitlist;

    /* First entry in the waitlist */
    if(waitlist->head_ptr == NULL) {
        waitlist->head_ptr = tcb;
        waitlist->tail_ptr = tcb;
        tcb->waitlist_next_ptr = NULL;
        return;
    }
    /* This task will be the next to have resource access */
    if(waitlist->head_ptr->priority < tcb->priority) {
        tcb->waitlist_next_ptr = waitlist->head_ptr;
        waitlist->head_ptr = tcb;
        return;
    }
    /* This task will be the last to get resource access */
    if(waitlist->tail_ptr->priority >= tcb->priority){
        tcb->waitlist_next_ptr = NULL;
        waitlist->tail_ptr->waitlist_next_ptr = tcb;
        waitlist->tail_ptr = tcb;
        return;
    }
    /* Add to the middle of the waitlist in prioity order */
    tcb2 = waitlist->head_ptr;
    tcb1 = tcb2->waitlist_next_ptr;
    while(tcb1->priority >= tcb->priority){
        tcb2 = tcb1;
        tcb1 = tcb1->waitlist_next_ptr;
    }
    tcb2->waitlist_next_ptr = tcb;
    tcb->waitlist_next_ptr = tcb1;
}