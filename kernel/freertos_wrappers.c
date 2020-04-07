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
#include "list.h"
#include "verios.h"
#include "task.h"
#include "schedule.h"
#include "verios_wrappers.h"

BaseType_t xTaskCreatePinnedToCore(	TaskFunction_t pvTaskCode,
										const char * const pcName,
										const uint32_t usStackDepth,
										void * const pvParameters,
										UBaseType_t uxPriority,
										TaskHandle_t * const pvCreatedTask,
										const BaseType_t xCoreID){
    int res = OS_task_create((TaskFunc_t) pvTaskCode,
							 pvParameters,
							 pcName,
							 (TaskPrio_t)uxPriority,
							 usStackDepth,
							 0,
							 xCoreID,
							 pvCreatedTask);
	if(res == 0){
		return pdPASS;
	}
	return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
}

BaseType_t xTaskCreateRestricted( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask ){
	configASSERT(0 == 1);
	return 0;
}

void vTaskAllocateMPURegions( TaskHandle_t xTask, const MemoryRegion_t * const pxRegions ){
	configASSERT(0 == 1);
}


void vTaskDelay( const TickType_t xTicksToDelay ) {
	OS_schedule_delay_task(xTicksToDelay);
}

void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement ) {
	configASSERT(0 == 1);
}

UBaseType_t uxTaskPriorityGet( TaskHandle_t xTask ) {
	configASSERT(0 == 1);
	return (UBaseType_t)OS_task_get_priority((TCB_t*)xTask);	
}

UBaseType_t uxTaskPriorityGetFromISR( TaskHandle_t xTask ) {
	configASSERT(0 == 1);
}

eTaskState eTaskGetState( TaskHandle_t xTask ) {
	configASSERT(0 == 1);
	return eRunning;
}

void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority ) {
	configASSERT(0 == 1);
	OS_schedule_change_task_prio((TCB_t *)xTask, (TaskPrio_t)uxNewPriority);
}

void vTaskSuspend( TaskHandle_t xTaskToSuspend ) {
	configASSERT(0 == 1);	
}

void vTaskResume( TaskHandle_t xTaskToResume ) {
	configASSERT(0 == 1);	
}

BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume ) {
	configASSERT(0 == 1);	
	return 0;
}

void vTaskStartScheduler( void ) {
	OS_schedule_start();
}

void vTaskEndScheduler( void ) {
	configASSERT(0 == 1);
	OS_schedule_stop();	
}

void vTaskSuspendAll( void ) {
	OS_schedule_suspend();
}

BaseType_t xTaskResumeAll( void ) {
	return (BaseType_t)OS_schedule_resume();	
}

TickType_t xTaskGetTickCount( void ) {
	return OS_schedule_get_tick_count();
}

TickType_t xTaskGetTickCountFromISR( void ) {
	configASSERT(0 == 1);
	return (TickType_t)0;	
}

UBaseType_t uxTaskGetNumberOfTasks( void ) {
	configASSERT(0 == 1);
	return 0;	
}

char *pcTaskGetTaskName( TaskHandle_t xTaskToQuery ) {
	return OS_task_get_name((TCB_t*)xTaskToQuery);
}

UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask ) {
	configASSERT(0 == 1);
	return 0;
}

uint8_t* pxTaskGetStackStart( TaskHandle_t xTask) {
	configASSERT(0 == 1);	
	return (uint8_t*)NULL;
}

void vTaskSetThreadLocalStoragePointer( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue ) {
	configASSERT(0 == 1);	
}

void *pvTaskGetThreadLocalStoragePointer( TaskHandle_t xTaskToQuery, BaseType_t xIndex ) {
	configASSERT(0 == 1);
}

void vTaskSetThreadLocalStoragePointerAndDelCallback( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue, TlsDeleteCallbackFunction_t pvDelCallback){
	configASSERT(0 == 1);
}

BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void *pvParameter ) {
	configASSERT(0 == 1);
	return -1;
}

TaskHandle_t xTaskGetIdleTaskHandle( void ) {
	return OS_schedule_get_idle_tcb(xPortGetCoreID());
}

TaskHandle_t xTaskGetIdleTaskHandleForCPU( UBaseType_t cpuid ) {
	return OS_schedule_get_idle_tcb(cpuid);
}

UBaseType_t uxTaskGetSystemState( TaskStatus_t * const pxTaskStatusArray, const UBaseType_t uxArraySize, uint32_t * const pulTotalRunTime ){
	configASSERT(0 == 1);
	return -1;
}

void vTaskList( char * pcWriteBuffer ){
	configASSERT(0 == 1);
}

void vTaskGetRunTimeStats( char *pcWriteBuffer){
	configASSERT(0 == 1);
} 

BaseType_t xTaskNotify( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction ){
	configASSERT(0 == 1);
	return -1;
}

BaseType_t xTaskNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, BaseType_t *pxHigherPriorityTaskWoken ){
	configASSERT(0 == 1);
	return -1;
}

BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait ){
	configASSERT(0 == 1);
	return -1;
}

#define xTaskNotifyGive( xTaskToNotify ) xTaskNotify( ( xTaskToNotify ), 0, eIncrement )

void vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken ){
	configASSERT(0 == 1);
}

uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait ){
	configASSERT(0 == 1);
	return 0;
}

BaseType_t xTaskIncrementTick( void ){
	return OS_schedule_process_tick();
}

void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait ){
	OS_schedule_place_task_on_event_list(pxEventList, xTicksToWait);
}

void vTaskPlaceOnUnorderedEventList( List_t * pxEventList, const TickType_t xItemValue, const TickType_t xTicksToWait ){
	configASSERT(0 == 1);
}

void vTaskPlaceOnEventListRestricted( List_t * const pxEventList, const TickType_t xTicksToWait ){
	OS_schedule_place_task_on_events_list_restricted(pxEventList, xTicksToWait);
}

BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList ){
	configASSERT(0 == 1);
	return -1;
}

BaseType_t xTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem, const TickType_t xItemValue ){
	configASSERT(0 == 1);
	return -1;
}

void vTaskSwitchContext( void ){
	OS_schedule_switch_context();	
}

TickType_t uxTaskResetEventItemValue( void ){
	configASSERT(0 == 1);
	return (TickType_t)-1;
}

TaskHandle_t xTaskGetCurrentTaskHandle( void ){
	return (TaskHandle_t)OS_schedule_get_current_tcb();
}

TaskHandle_t xTaskGetCurrentTaskHandleForCPU( BaseType_t cpuid ){
	return (TaskHandle_t)OS_schedule_get_current_tcb_from_core(cpuid);
}

void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut ){
	OS_set_timeout_state(pxTimeOut);
}

BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait ){
	OSBool_t ret_val = OS_schedule_check_for_timeout(pxTimeOut, pxTicksToWait);
	if(ret_val == OS_FALSE){
		return pdFALSE;
	}
	return pdTRUE;
}

void vTaskMissedYield( void ){
    configASSERT(0 == 1);
}

BaseType_t xTaskGetSchedulerState( void ){
	OSScheduleState_t state = OS_schedule_get_state();
	if(state == OS_SCHEDULE_STATE_STOPPED){
		return taskSCHEDULER_NOT_STARTED;
	} else if (state == OS_SCHEDULE_STATE_RUNNING) {
		return taskSCHEDULER_RUNNING;
	} else if(state == OS_SCHEDULE_STATE_SUSPENDED){
		return taskSCHEDULER_SUSPENDED;
	}
	return -1;
}

void vTaskPriorityInherit( TaskHandle_t const pxMutexHolder ) {
	configASSERT(0 == 1);
	return;
}

UBaseType_t uxTaskGetTaskNumber( TaskHandle_t xTask ) {
	configASSERT(0 == 1);
	return -1;
}

BaseType_t xTaskGetAffinity( TaskHandle_t xTask ) {
	return OS_task_get_core_ID((TCB_t *)xTask);
}

void vTaskSetTaskNumber( TaskHandle_t xTask, const UBaseType_t uxHandle ) {
	configASSERT(0 == 1);
}

void vTaskStepTick( const TickType_t xTicksToJump ){
	configASSERT(0 == 1);
}

eSleepModeStatus eTaskConfirmSleepModeStatus( void ){
	configASSERT(0 == 1);
	return eAbortSleep;
}

void *pvTaskIncrementMutexHeldCount( void ){
	return OS_schedule_increment_task_mutex_count();
}

UBaseType_t uxTaskGetSnapshotAll( TaskSnapshot_t * const pxTaskSnapshotArray, const UBaseType_t uxArraySize, UBaseType_t * const pxTcbSz ){
	configASSERT(0 == 1);
	return -1;
}

