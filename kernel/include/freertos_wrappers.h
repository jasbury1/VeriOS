#ifndef OS_VERIOS_WRAPPERS_H
#define OS_VERIOS_WRAPPERS_H


#include <limits.h>
#include <stdint.h>

#include "FreeRTOS_old.h"
#include "list.h"
#include "freertos/portmacro.h"
#include "verios.h"
#include "task.h"
#include "schedule.h"

/*-----------------------------------------------------------
 * MACROS AND DEFINITIONS
 *----------------------------------------------------------*/

#define tskKERNEL_VERSION_NUMBER "V8.2.0"
#define tskKERNEL_VERSION_MAJOR 8
#define tskKERNEL_VERSION_MINOR 2
#define tskKERNEL_VERSION_BUILD 0

#define taskSCHEDULER_SUSPENDED		( ( BaseType_t ) 0 )
#define taskSCHEDULER_NOT_STARTED	( ( BaseType_t ) 1 )
#define taskSCHEDULER_RUNNING		( ( BaseType_t ) 2 )

#define tskNO_AFFINITY CORE_NO_AFFINITY

#define tskIDLE_PRIORITY OS_IDLE_PRIORITY

typedef void * TaskHandle_t;

typedef BaseType_t (*TaskHookFunction_t)( void * );

typedef void (*TlsDeleteCallbackFunction_t)( int, void * );

typedef enum
{
	eRunning = 0,	/*!< A task is querying the state of itself, so must be running. */
	eReady,			/*!< The task being queried is in a read or pending ready list. */
	eBlocked,		/*!< The task being queried is in the Blocked state. */
	eSuspended,		/*!< The task being queried is in the Suspended state, or is in the Blocked state with an infinite time out. */
	eDeleted		/*!< The task being queried has been deleted, but its TCB has not yet been freed. */
} eTaskState;

typedef enum
{
	eNoAction = 0,				/*!< Notify the task without updating its notify value. */
	eSetBits,					/*!< Set bits in the task's notification value. */
	eIncrement,					/*!< Increment the task's notification value. */
	eSetValueWithOverwrite,		/*!< Set the task's notification value to a specific value even if the previous value has not yet been read by the task. */
	eSetValueWithoutOverwrite	/*!< Set the task's notification value if the previous value has been read by the task. */
} eNotifyAction;

typedef struct xTASK_PARAMETERS
{
	TaskFunction_t pvTaskCode;
	const char * const pcName;	/*lint !e971 Unqualified char types are allowed for strings and single characters only. */
	uint32_t usStackDepth;
	void *pvParameters;
	UBaseType_t uxPriority;
	StackType_t *puxStackBuffer;
	MemoryRegion_t xRegions[ portNUM_CONFIGURABLE_REGIONS ];
} TaskParameters_t;

typedef struct xTASK_STATUS
{
	TaskHandle_t xHandle;			/*!< The handle of the task to which the rest of the information in the structure relates. */
	const char *pcTaskName;			/*!< A pointer to the task's name.  This value will be invalid if the task was deleted since the structure was populated! */ /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
	UBaseType_t xTaskNumber;		/*!< A number unique to the task. */
	eTaskState eCurrentState;		/*!< The state in which the task existed when the structure was populated. */
	UBaseType_t uxCurrentPriority;	/*!< The priority at which the task was running (may be inherited) when the structure was populated. */
	UBaseType_t uxBasePriority;		/*!< The priority to which the task will return if the task's current priority has been inherited to avoid unbounded priority inversion when obtaining a mutex.  Only valid if configUSE_MUTEXES is defined as 1 in FreeRTOSConfig.h. */
	uint32_t ulRunTimeCounter;		/*!< The total run time allocated to the task so far, as defined by the run time stats clock.  See http://www.freertos.org/rtos-run-time-stats.html.  Only valid when configGENERATE_RUN_TIME_STATS is defined as 1 in FreeRTOSConfig.h. */
	StackType_t *pxStackBase;		/*!< Points to the lowest address of the task's stack area. */
	uint32_t usStackHighWaterMark;	/*!< The minimum amount of stack space that has remained for the task since the task was created.  The closer this value is to zero the closer the task has come to overflowing its stack. */
} TaskStatus_t;

typedef struct xTASK_SNAPSHOT
{
	void        *pxTCB;         /*!< Address of task control block. */
	StackType_t *pxTopOfStack;  /*!< Points to the location of the last item placed on the tasks stack. */
	StackType_t *pxEndOfStack;  /*!< Points to the end of the stack. pxTopOfStack < pxEndOfStack, stack grows hi2lo
									pxTopOfStack > pxEndOfStack, stack grows lo2hi*/
} TaskSnapshot_t;

typedef enum
{
	eAbortSleep = 0,		/*!< A task has been made ready or a context switch pended since portSUPPORESS_TICKS_AND_SLEEP() was called - abort entering a sleep mode. */
	eStandardSleep,			/*!< Enter a sleep mode that will not last any longer than the expected idle time. */
	eNoTasksWaitingTimeout	/*!< No tasks are waiting for a timeout so it is safe to enter a sleep mode that can only be exited by an external interrupt. */
} eSleepModeStatus;



BaseType_t xTaskCreatePinnedToCore(	TaskFunction_t pvTaskCode,
										const char * const pcName,
										const uint32_t usStackDepth,
										void * const pvParameters,
										UBaseType_t uxPriority,
										TaskHandle_t * const pvCreatedTask,
										const BaseType_t xCoreID);

static inline IRAM_ATTR BaseType_t xTaskCreate(
			TaskFunction_t pvTaskCode,
			const char * const pcName,
			const uint32_t usStackDepth,
			void * const pvParameters,
			UBaseType_t uxPriority,
			TaskHandle_t * const pvCreatedTask)
{
	return xTaskCreatePinnedToCore( pvTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pvCreatedTask, tskNO_AFFINITY );
}

BaseType_t xTaskCreateRestricted( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask );

void vTaskAllocateMPURegions( TaskHandle_t xTask, const MemoryRegion_t * const pxRegions );

static inline IRAM_ATTR void vTaskDelete( TaskHandle_t xTaskToDelete ){
	OS_task_delete((TCB_t *)xTaskToDelete);
}

void vTaskDelay( const TickType_t xTicksToDelay );

void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement );

UBaseType_t uxTaskPriorityGet( TaskHandle_t xTask );

UBaseType_t uxTaskPriorityGetFromISR( TaskHandle_t xTask );

eTaskState eTaskGetState( TaskHandle_t xTask );

void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority );

void vTaskSuspend( TaskHandle_t xTaskToSuspend );

void vTaskResume( TaskHandle_t xTaskToResume );

BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume );

void vTaskStartScheduler( void );

void vTaskEndScheduler( void );

void vTaskSuspendAll( void );

BaseType_t xTaskResumeAll( void );

TickType_t xTaskGetTickCount( void );

TickType_t xTaskGetTickCountFromISR( void );

UBaseType_t uxTaskGetNumberOfTasks( void );

char *pcTaskGetTaskName( TaskHandle_t xTaskToQuery );

UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask );

uint8_t* pxTaskGetStackStart( TaskHandle_t xTask);

void vTaskSetThreadLocalStoragePointer( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue );

void *pvTaskGetThreadLocalStoragePointer( TaskHandle_t xTaskToQuery, BaseType_t xIndex );

void vTaskSetThreadLocalStoragePointerAndDelCallback( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue, TlsDeleteCallbackFunction_t pvDelCallback);

BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void *pvParameter );

TaskHandle_t xTaskGetIdleTaskHandle( void );

TaskHandle_t xTaskGetIdleTaskHandleForCPU( UBaseType_t cpuid );

UBaseType_t uxTaskGetSystemState( TaskStatus_t * const pxTaskStatusArray, const UBaseType_t uxArraySize, uint32_t * const pulTotalRunTime );

void vTaskList( char * pcWriteBuffer );

void vTaskGetRunTimeStats( char *pcWriteBuffer);

BaseType_t xTaskNotify( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction );

BaseType_t xTaskNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, BaseType_t *pxHigherPriorityTaskWoken );

BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait );

#define xTaskNotifyGive( xTaskToNotify ) xTaskNotify( ( xTaskToNotify ), 0, eIncrement )

void vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken );

uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait );

BaseType_t xTaskIncrementTick( void );

void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait );

void vTaskPlaceOnUnorderedEventList( List_t * pxEventList, const TickType_t xItemValue, const TickType_t xTicksToWait );

void vTaskPlaceOnEventListRestricted( List_t * const pxEventList, const TickType_t xTicksToWait );

BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList );

BaseType_t xTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem, const TickType_t xItemValue );

void vTaskSwitchContext( void );

TickType_t uxTaskResetEventItemValue( void );

TaskHandle_t xTaskGetCurrentTaskHandle( void );

TaskHandle_t xTaskGetCurrentTaskHandleForCPU( BaseType_t cpuid );

void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut );

BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait ) PRIVILEGED_FUNCTION;

void vTaskMissedYield( void ) PRIVILEGED_FUNCTION;

BaseType_t xTaskGetSchedulerState( void );

void vTaskPriorityInherit( TaskHandle_t const pxMutexHolder );

static inline IRAM_ATTR BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder ){
	return (BaseType_t)(OS_schedule_revert_priority_mutex_holder((void * const)pxMutexHolder));
}

UBaseType_t uxTaskGetTaskNumber( TaskHandle_t xTask );

BaseType_t xTaskGetAffinity( TaskHandle_t xTask );

void vTaskSetTaskNumber( TaskHandle_t xTask, const UBaseType_t uxHandle );

void vTaskStepTick( const TickType_t xTicksToJump );

eSleepModeStatus eTaskConfirmSleepModeStatus( void );

void *pvTaskIncrementMutexHeldCount( void );

UBaseType_t uxTaskGetSnapshotAll( TaskSnapshot_t * const pxTaskSnapshotArray, const UBaseType_t uxArraySize, UBaseType_t * const pxTcbSz );

#endif /* OS_VERIOS_WRAPPERS_H */