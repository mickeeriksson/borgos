#ifndef _KERNEL_TYPES_H_
#define _KERNEL_TYPES_H_

#include "cpu/cpu_types.h"

#ifndef NULL
    #define NULL 0
#endif

/*
typedef uint32_t kdev_t;
typedef uint32_t offset_t;
typedef uint16_t umode_t;
*/
typedef int RESULT;

typedef int BOOL;
#define TRUE 1
#define FALSE 0

/** Type-independent macro to calculate the minimum of 2 values.  Stolen from XINU  */
#define MIN(a, b) ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); (_a < _b) ? _a : _b; })

/** Type-independent macro to calculate the maximum of 2 values.  Stolen from XINU  */
#define MAX(a, b) ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); (_a > _b) ? _a : _b; })



#endif