/**
 * 
 * 
 */
 
 #ifndef OS_TIMER_H
 #define OS_TIMER_H

 #include "task.h"
 #include "schedule.h"


/* IDs for commands that can be sent/received on the timer queue.  These are to
be used solely through the macros that make up the public software timer API,
as defined below.  The commands that are sent from interrupts must use the
highest numbers as tmrFIRST_FROM_ISR_COMMAND is used to determine if the task
or interrupt version of the queue send function should be used. */
#define OS_TIMER_COMMAND_EXECUTE_CALLBACK_FROM_ISR -2 
#define OS_TIMER_COMMAND_EXECUTE_CALLBACK	-1
#define OS_TIMER_COMMAND_START_DONT_TRACE 0
#define OS_TIMER_COMMAND_START 1 
#define OS_TIMER_COMMAND_RESET 2 
#define OS_TIMER_COMMAND_STOP 3
#define OS_TIMER_COMMAND_CHANGE_PERIOD 4	
#define OS_TIMER_COMMAND_DELETE 5

#define OS_TIMER_FIRST_FROM_ISR_COMMAND 6
#define OS_TIMER_COMMAND_START_FROM_ISR 6
#define OS_TIMER_COMMAND_RESET_FROM_ISR 7
#define OS_TIMER_COMMAND_STOP_FROM_ISR 8
#define OS_TIMER_COMMAND_CHANGE_PERIOD_FROM_ISR 9 

typedef void * TimerHandle_t;

typedef void (*TimerCallbackFunction_t)(TimerHandle_t timer_handle);

typedef void (*PendedFunction_t)(void *, uint32_t);

TimerHandle_t OS_timer_create( const char * timer_name,
                               const TickType_t timer_period_ticks,
                               const uint8_t restart_after_finishing,
                               void * const timer_ID,
                               TimerCallbackFunction_t callback_function) PRIVILEGED_FUNCTION;

void OS_timer_get_ID(TimerHandle_t timer_handle) PRIVILEGED_FUNCTION;

void OS_timer_set_ID(TimerHandle_t timer_handle, void *new_ID) PRIVILEGED_FUNCTION;

uint8_t OS_timer_is_active(TimerHandle_t timer_handle) PRIVILEGED_FUNCTION;

void * OS_timer_get_daemon_task_handle(void);

TickType_t OS_timer_get_period(TimerHandle_t timer_handle) PRIVILEGED_FUNCTION;

TickType_t OS_timer_get_expiry_time(TimerHandle_t timer_handle) PRIVILEGED_FUNCTION;

#define OS_timer_start( timer_handle, ticks_to_wait ) OS_timer_generic_command( ( timer_handle ), OS_TIMER_COMMAND_START, ( OS_task_get_tick_count() ), NULL, ( ticks_to_wait ) )

#define OS_timer_stop( timer_handle, ticks_to_wait ) OS_timer_generic_command( ( timer_handle ), OS_TIMER_COMMAND_STOP, 0U, NULL, ( ticks_to_wait ) )

#define OS_timer_change_period( timer_handle, new_period, ticks_to_wait ) OS_timer_generic_command( ( timer_handle ), OS_TIMER_COMMAND_CHANGE_PERIOD, ( new_period ), NULL, ( ticks_to_wait ) )

#define OS_timer_delete( timer_handle, ticks_to_wait ) OS_timer_generic_command( ( timer_handle ), OS_TIMER_COMMAND_DELETE, 0U, NULL, ( timer_handle ) )

#define OS_timer_reset( timer_handle, ticks_to_wait ) OS_timer_generic_command( ( timer_handle ), OS_TIMER_COMMAND_RESET, ( OS_task_get_tick_count() ), NULL, ( ticks_to_wait ) )

#define OS_timer_start_from_ISR( timer_handle, higher_priority_task_woken ) OS_timer_generic_command( ( timer_handle ), OS_TIMER_COMMAND_START_FROM_ISR, ( OS_task_get_tick_count_from_ISR() ), ( higher_priority_task_woken ), 0U )

#define OS_timer_stop_from_ISR( timer_handle, higher_priority_task_woken ) OS_timer_generic_command( ( timer_handle ), OS_TIMER_COMMAND_STOP_FROM_ISR, 0, ( pxHigherPriorityTaskWoken ), 0U )

#define OS_timer_change_period_from_ISR( timer_handle, new_handle, higher_priority_task_woken ) OS_timer_generic_command( ( timer_handle ), OS_TIMER_COMMAND_CHANGE_PERIOD_FROM_ISR, ( new_period ), ( higher_priority_task_woken ), 0U )

#define OS_timer_reset_from_ISR( timer_handle, higher_priority_task_woken ) OS_timer_generic_command( ( timer_handle ), OS_TIMER_COMMAND_RESET_FROM_ISR, ( OS_task_get_tick_count_from_ISR() ), ( higher_priority_task_woken ), 0U )

int OS_timer_pend_function_call_from_ISR( PendedFunction_t function_to_pend, void *param1, uint32_t param2, int *higher_priority_task_woken );

int OS_timer_pend_function_call( PendedFunction_t function_to_pend, void *param1, uint32_t param2, TickType_t ticks_to_wait );

const char * OS_timer_get_name(TimerHandle_t timer_handle);

int OS_timer_create_task(void) PRIVILEGED_FUNCTION;
int OS_timer_generic_command(TimerHandle_t timer_handle, const int command_ID, const TickType_t optional_value, int * const higher_priority_task_woken, const TickType_t ticks_to_wait) PRIVILEGED_FUNCTION;


 #endif /* OS_TIMER_H */ 