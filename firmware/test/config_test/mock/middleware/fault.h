/* Mock middleware/fault.h */
#ifndef MIDDLEWARE_FAULT_H
#define MIDDLEWARE_FAULT_H

#include <stdint.h>

typedef enum {
    FAULT_LEVEL_INFO = 0,
    FAULT_LEVEL_WARN,
    FAULT_LEVEL_ERROR,
    FAULT_LEVEL_FATAL
} fault_level_t;

void fault_record(fault_level_t level, const char* module, const char* msg);

#endif
