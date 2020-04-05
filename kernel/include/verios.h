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

/*******************************************************************************
* MACROS
*******************************************************************************/

#define OS_TRUE (uint8_t)1
#define OS_FALSE (uint8_t)0

#define OS_MAX_PRIORITIES 256

#ifndef OS_PRIVILEGE_BIT
    #define OS_PRIVILEGE_BIT ((uint8_t) 0x00)
#endif /* OS_PRIVILEGE_BIT */ 

/*******************************************************************************
* TYPEDEFS AND DATA STRUCTURES
*******************************************************************************/

typedef uint8_t OSBool_t;

typedef enum OS_error_codes {
    OS_NO_ERROR = 0,

    OS_OTHER_ERROR
} OSError_t;

#endif /* VERIOS_H */