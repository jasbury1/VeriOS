/**
 * 
 * HEADER GOES HERE- make it look cool later
 *
 *
 *
 *
 */

/* Standard includes */
#include <stdlib.h>
#include <stdint.h>

/* OS specific includes */
#include "include/task.h"
#include "include/schedule.h"
#include "include/verios.h"
#include "../cpu/xtensa/portmacro.h"

PRIVILEGED_DATA TCB_t * volatile OS_current_TCB[ portNUM_PROCESSORS ] = { NULL };

PRIVILEGED_DATA static volatile unsigned int OS_num_tasks = 0;
PRIVILEGED_DATA static volatile uint8_t scheduler_running = OS_FALSE;

PRIVILEGED_DATA static portMUX_TYPE OS_schedule_mutex = portMUX_INITIALIZER_UNLOCKED

/**
 * Bitmap of priorities in use.
 * Broken up into an array of 8 bit integers where each bit represents if a priority is in use
 * The map is designed backwards. 
 * The first bit corresponds to the highest priority (highest number)
 * The last bit is priority 0 (reserved for Idle)
 */
static uint8_t OS_ready_priorities_map[OS_PRIO_MAP_SIZE];

/**
 * The Ready list of all tasks ready to be run
 * Each index corresponds to a priority. Index 0 is priority 0 (reserved for Idle)
 */
static ReadyList_t OS_ready_list[OS_MAX_PRIORITIES];



/**
 * Zeros out the bit map storing priorities that are in use
 */
void _OS_schedule_reset_prio_map(void){
    int i;
    for(i = 0; i < OS_PRIO_MAP_SIZE; ++i){
        OS_ready_priorities_map[i] = (uint8_t)0;
    }
}

/**
 * Return the highest priority that is currently assigned to any task
 */
int _OS_schedule_get_highest_prio(void){
    uint8_t val;
    int leading_zeros = 0;
    int map_index = -1;

    while(OS_ready_priorities_map[++map_index] == (uint8_t)0 && map_index < OS_PRIO_MAP_SIZE);

    if(map_index == OS_PRIO_MAP_SIZE){
        /* TODO */
        /* No priorities in use in the map */
    }

    /* Count the leading zeros in the bit map index that contains a priority */
    val = OS_ready_priorities_map[map_index];
    while(!(val & (1 << 7))){
        val = (val << 1);
        ++leading_zeros;
    }

    return((OS_MAX_PRIORITIES - 1) - ((map_index * 8) + leading_zeros));
}

/**
 * Add an entry to the priority bitmap corresponding to the new priority
 * Does nothing if the bit is already set to 1
 */
void _OS_schedule_add_prio(int new_prio)
{
    int index = (int)((OS_MAX_PRIORITIES - new_prio) / 8);
    int shift = (OS_MAX_PRIORITIES - new_prio) % 8;

    OS_ready_priorities_map[index] = OS_ready_priorities_map | ((uint8_t)128 >> shift);
}

/**
 * Remove the entry in the bitmap corresponding to the given priority
 * Should only be done if no tasks use that priority anymore
 */
void _OS_schedule_remove_prio(int prio)
{
    int index = (int)((OS_MAX_PRIORITIES - prio) / 8);
    int shift = (OS_MAX_PRIORITIES - prio) % 8;

    OS_ready_priorities_map[index] = OS_ready_priorities_map ^ ((uint8_t)128 >> shift);
}

/**
 * Ensures that all entries in the ready list are reset
 */
void _OS_schedule_ready_list_init(void)
{
    int index;
    for(index = 0; index < OS_MAX_PRIORITIES; ++i){
        OS_ready_list[i]->num_tasks = 0;
        OS_ready_list[i]->head_ptr = NULL;
        OS_ready_list[i]->tail_ptr = NULL;
    }
}

/**
 * Responsible for placing the tcb in the ready list
 * Application code should not call! This is a helper for OS_add_task_to_ready_list
 */
void _OS_schedule_ready_list_insert(TCB_t *new_tcb)
{

}

/**
 * The main API function for adding to a ready list!
 * Handles all of the stuff
 */
void OS_add_task_to_ready_list(TCB_t *new_tcb, int core_ID)
{
    /* TODO : Ensure that the coreID is not invalid / out of range */
    portENTER_CRITICAL(&OS_schedule_mutex);
    
    /* If there is no affinity with which core it is placed on */
    if(core_ID == CORE_NO_AFFINITY) {
        /* Place it on the single existing core if we don't have multiple cores */
        core_ID = portNUM_PROCESSORS == 1 ? 0 : core_ID;
        /* Otherwise place it on a core. TODO: Better logic here in the future. Optimize */
        core_ID = xPortGetCoreID();
    }

    /* Increase the task count and update the ready list and priority bitmap */
    ++OS_num_tasks;
    _OS_schedule_add_prio(new_tcb->priority);
    _OS_schedule_ready_list_insert(new_tcb);

    if(scheduler_running == OS_FALSE) {
        /* TODO refer to line 1175 in task.c freeRTOS */
        portEXIT_CRITICAL(&OS_schedule_mutex)
        return;
    }

    portEXIT_CRITICAL(&OS_schedule_mutex);
}

/**
 * 
 */
void OS_schedule_update(void)
{
    
}