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

#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* OS specific includes */
#include "include/task.h"
#include "include/verios.h"
#include "../cpu/xtensa/portmacro.h"
#include "include/schedule.h"
#include "include/timer.h"

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#define OS_TIMER_NO_DELAY (TickType_t)0

typedef struct TimerControl {
    const char timer_name;
    TickType_t timer_period_ticks;
    uint8_t restart_after_finishing;
    void *timer_ID;
    TimerCallbackFunction_t callback_function;
} Timer_t;

typedef struct TimerParameters {
    TickType_t message_value;
    Timer_t * timer;
} TimerParam_t;

typedef struct TimerCallbackParameters {
    PendedFunction_t callback_function;
    void * param1;
    uint32_t param2;
} TimerCallbackParam_t;

typedef struct TimerQueueMessage {
    int message_ID;
    union {
        TimerParam_t timer_parameters;
        TimerCallbackParam_t callback_parameters;
    } u;
} DaemonTaskMessage_t
