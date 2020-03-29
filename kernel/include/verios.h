#ifndef VERIOS_H
#define VERIOS_H

#include <stddef.h>

/* Standard includes */
#include <limits.h>
#include <stdint.h>

/* for likely and unlikely */
#include "esp_compiler.h"

/* Application specific configuration options. */
#include "freertos/FreeRTOSConfig.h"

/* Basic FreeRTOS definitions. */
#include "projdefs.h"

/* Definitions specific to the port being used. */
#include "portable.h"

/* Standard macros */
#define OS_TRUE (uint8_t)1
#define OS_FALSE (uint8_t)0

#define OS_MAX_PRIORITIES 256

#ifndef OS_PRIVILEGE_BIT
    #define OS_PRIVILEGE_BIT ((uint8_t) 0x00)
#endif /* OS_PRIVILEGE_BIT */ 

/* TYPEDEFS */
typedef uint8_t OSBool_t;

#endif /* VERIOS_H */