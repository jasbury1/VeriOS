#ifndef VERIOS_H
#define VERIOS_H

/* Standard includes */
#include <limits.h>
#include <stdint.h>

/* Standard macros */
#define OS_TRUE (uint8_t)1
#define OS_FALSE (uint8_t)0

#define OS_MAX_PRIORITIES 256

#ifndef OS_PRIVILEGE_BIT
    #define OS_PRIVILEGE_BIT ((uint8_t) 0x00)
#endif /* OS_PRIVILEGE_BIT */ 

#endif /* VERIOS_H */