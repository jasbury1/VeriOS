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

#define OS_PRIO_MAP_SIZE (OS_MAX_PRIORITIES / 8)

volatile unsigned int OS_num_tasks = 0;

/**
 * Bitmap of priorities in use.
 * Broken up into an array of 8 bit integers where each bit represents if a priority is in use
 * The map is designed backwards. 
 * The first bit corresponds to the highest priority (highest number)
 * The last bit is priority 0 (reserved for Idle)
 */
uint8_t OS_prio_map[OS_PRIO_MAP_SIZE];

/**
 * The Ready list of all tasks ready to be run
 * Each index corresponds to a priority. Index 0 is priority 0 (reserved for Idle)
 */
ReadyList_t OS_ready_list[OS_MAX_PRIORITIES];

PRIVILEGED_DATA static volatile uint8_t scheduler_running = OS_FALSE;

/**
 * Zeros out the bit map storing priorities that are in use
 */
void _OS_schedule_reset_prio_map(void){
    int i;
    for(i = 0; i < OS_PRIO_MAP_SIZE; ++i){
        OS_prio_map[i] = (uint8_t)0;
    }
}

/**
 * Return the highest priority that is currently assigned to any task
 */
int _OS_schedule_get_highest_prio(void){
    uint8_t val;
    int leading_zeros = 0;
    int map_index = -1;

    while(OS_prio_map[++map_index] == (uint8_t)0 && map_index < OS_PRIO_MAP_SIZE);

    if(map_index == OS_PRIO_MAP_SIZE){
        /* TODO */
        /* No priorities in use in the map */
    }

    /* Count the leading zeros in the bit map index that contains a priority */
    val = OS_prio_map[map_index];
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
void _OS_schedule_add_prio(int new_prio){
    int index = (int)((OS_MAX_PRIORITIES - new_prio) / 8);
    int shift = (OS_MAX_PRIORITIES - new_prio) % 8;

    OS_prio_map[index] = OS_prio_map | ((uint8_t)128 >> shift);
}

/**
 * Remove the entry in the bitmap corresponding to the given priority
 * Should only be done if no tasks use that priority anymore
 */
void _OS_schedule_remove_prio(int prio){
    int index = (int)((OS_MAX_PRIORITIES - prio) / 8);
    int shift = (OS_MAX_PRIORITIES - prio) % 8;

    OS_prio_map[index] = OS_prio_map ^ ((uint8_t)128 >> shift);
}

void _OS_schedule_reset_list(void){

}
