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
#include "verios_util.h"
#include "task.h"

/*******************************************************************************
* STATIC FUNCTION DECLARATIONS
*******************************************************************************/

/*******************************************************************************
* OS Waitlist Insert Task
*
*   tcb = Pointer to the tcb that is getting added to a waitlist
*   waitlist = A pointer to the waitlist to add the task to
* 
* PURPOSE : 
*
*   Add the task to the waitlist specified. The waitlist is ordered based on the
*   decreasing task priority and the task will get added to the right spot
* 
* RETURN :
*   
*
* NOTES: 
*******************************************************************************/

void _OS_waitlist_append(TCB_t *tcb, WaitList_t *waitlist)
{
    TCB_t *tcb1;
    TCB_t *tcb2;

    /* Update the task counter for this delayed list */
    waitlist->num_tasks++;
    tcb->block_record.waitlist = waitlist;

    /* First entry in the waitlist */
    if(waitlist->head_ptr == NULL) {
        waitlist->head_ptr = tcb;
        waitlist->tail_ptr = tcb;
        tcb->block_record.waitlist_next_ptr = NULL;
        tcb->block_record.waitlist_prev_ptr = NULL;
        return;
    }
    /* This task will be the next to have resource access */
    if(waitlist->head_ptr->priority < tcb->priority) {
        tcb->block_record.waitlist_next_ptr = waitlist->head_ptr;
        tcb->block_record.waitlist_prev_ptr = NULL;
        waitlist->head_ptr = tcb;
        return;
    }
    /* This task will be the last to get resource access */
    if(waitlist->tail_ptr->priority >= tcb->priority){
        tcb->block_record.waitlist_next_ptr = NULL;
        tcb->block_record.waitlist_prev_ptr = waitlist->tail_ptr;
        waitlist->tail_ptr->block_record.waitlist_next_ptr = tcb;
        waitlist->tail_ptr = tcb;
        return;
    }
    /* Add to the middle of the waitlist in prioity order */
    tcb2 = waitlist->head_ptr;
    tcb1 = tcb2->block_record.waitlist_next_ptr;
    while(tcb1->priority >= tcb->priority){
        tcb2 = tcb1;
        tcb1 = tcb1->block_record.waitlist_next_ptr;
    }
    tcb2->block_record.waitlist_next_ptr = tcb;
    tcb->block_record.waitlist_prev_ptr = tcb2;
    tcb->block_record.waitlist_next_ptr = tcb1;
}

/*******************************************************************************
* OS Waitlist Insert Task
*
*   tcb = Pointer to the tcb that is getting added to a waitlist
*   waitlist = A pointer to the waitlist to add the task to
* 
* PURPOSE : 
*
*   Add the task to the waitlist specified. The waitlist is ordered based on the
*   decreasing task priority and the task will get added to the right spot
* 
* RETURN :
*   
*
* NOTES: 
*******************************************************************************/

void _OS_waitlist_remove(TCB_t *tcb)
{
    WaitList_t *waitlist = tcb->block_record.waitlist;
    assert(waitlist);
    
    /* Removing the only task */
    if(waitlist->num_tasks == 1) {
        waitlist->head_ptr = NULL;
        waitlist->tail_ptr = NULL;
    }
    /* Removing the head */
    else if(tcb == waitlist->head_ptr){
        waitlist->head_ptr = tcb->block_record.waitlist_next_ptr;
        waitlist->head_ptr->block_record.waitlist_prev_ptr = NULL;
    }
    /* Removing the tail */
    else if(tcb == waitlist->tail_ptr){
        waitlist->tail_ptr = tcb->block_record.waitlist_prev_ptr;
        waitlist->tail_ptr->block_record.waitlist_next_ptr = NULL;
    }
    /* Removing in the middle */
    else {
        configASSERT(waitlist->num_tasks > 2);
        tcb->block_record.waitlist_next_ptr->block_record.waitlist_prev_ptr = tcb->block_record.waitlist_prev_ptr;
        tcb->block_record.waitlist_prev_ptr->block_record.waitlist_next_ptr = tcb->block_record.waitlist_next_ptr;
    }

    waitlist->num_tasks--;
    tcb->block_record.waitlist_next_ptr = NULL;
    tcb->block_record.waitlist_prev_ptr = NULL;
    tcb->block_record.waitlist = NULL;
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
*
*   This function will not do any scheduling on the task after it is popped.
*   This is left to the caller 
*******************************************************************************/

TCB_t * _OS_waitlist_pop_head(WaitList_t *waitlist)
{
    TCB_t *head;
    assert(waitlist->head_ptr);

    head = waitlist->head_ptr;

    if(waitlist->num_tasks == 1){
        waitlist->head_ptr = NULL;
        waitlist->tail_ptr = NULL;
        waitlist->num_tasks = 0;
    }
    else {
        waitlist->head_ptr = waitlist->head_ptr->block_record.waitlist_next_ptr;
        waitlist->num_tasks--;
    }
    /* This task is no longer on a waitlist */
    head->block_record.waitlist = NULL;
    head->block_record.waitlist_next_ptr = NULL;
    head->block_record.waitlist_prev_ptr = NULL;

    return head;
}

void _OS_task_list_remove(TCB_t* tcb, volatile struct OSTaskListHeader * task_list)
{
   /* Removing the only task */
    if(task_list->num_tasks == 1){
        task_list->head_ptr = NULL;
        task_list->tail_ptr = NULL;
    }
    /* Removing the head */
    else if(tcb == task_list->head_ptr){
        task_list->head_ptr = tcb->next_ptr;
        task_list->head_ptr->prev_ptr = NULL;
    }
    /* Removing the tail */
    else if(tcb == task_list->tail_ptr){
        task_list->tail_ptr = tcb->prev_ptr;
        task_list->tail_ptr->next_ptr = NULL;
    }
    /* Removing in the middle */
    else {
        configASSERT(task_list->num_tasks > 2);
        configASSERT(tcb->next_ptr != NULL);
        configASSERT(tcb->prev_ptr != NULL);
        tcb->next_ptr->prev_ptr = tcb->prev_ptr;
        tcb->prev_ptr->next_ptr = tcb->next_ptr;
    }
    tcb->next_ptr = NULL;
    tcb->prev_ptr = NULL;
    task_list->num_tasks--; 
}

void _OS_task_list_append(TCB_t *tcb, volatile struct OSTaskListHeader * task_list)
{
    assert(tcb->next_ptr == NULL);
    assert(tcb->prev_ptr == NULL);

    /* First entry */
    if(task_list->num_tasks == 0){
        task_list->head_ptr = tcb;
        task_list->tail_ptr = tcb;
        task_list->num_tasks = 1;
        return;
    }
    /* Add to the end */
    task_list->tail_ptr->next_ptr = tcb;
    tcb->prev_ptr = task_list->tail_ptr;
    task_list->tail_ptr = tcb;
    task_list->num_tasks++; 
}