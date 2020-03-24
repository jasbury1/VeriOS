/**
 * 
 * 
 */
 
 #ifndef OS_TASK_H
 #define OS_TASK_H

 #include <limits.h>
 #include <stdint.h>

 #define CORE_NO_AFFINITY -1

 #define OS_IDLE_STACK_SIZE configIDLE_TASK_SIZE

/**
 * The Task Control Block
 * Kernel bookkeeping on each task created
 * 
 * Should NEVER be used by user code
 */
typedef struct OSTaskControlBlock
{
    /* Pointer to last element pushed to stack */
    volatile StackType_t *stack_top;

    /* Set to OS_TRUE if the task was statically allocated and doesn't require freeing */
    uint8_t is_static;

    /* The core being used for multi-core systems */
    int core_ID;

    TaskPrio_t priority;
    StackType_t stack_start;
    StackType_t stack_end;
    int stack_size;
    char * task_name;

    /* IPC data */
    int msg_queue_size;

    int mutexes_held;

    /* List data */
    TCB_t *next_ptr;
    TCB_t *prev_ptr;


} TCB_t;

/* Specity the size of the integer for a task priority */
typedef uint8_t TaskPrio_t;

/* The function pointer representing a task function */
/* TODO: Should be moved to a better header file */
typedef void (*TaskFunc_t)( void * );



 #endif /* OS_TASK_H */ 