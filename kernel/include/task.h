/**
 * 
 * 
 */
 
 #ifndef OS_TASK_H
 #define OS_TASK_H

 #include <limits.h>
 #include <stdint.h>

/* Specity the size of the integer for a task priority */
typedef uint8_t TaskPrio_t;

/* The function pointer representing a task function */
/* TODO: Should be moved to a better header file */
typedef void (*TaskFunc_t)( void * );



 #endif /* OS_TASK_H */ 