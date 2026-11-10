#ifndef APP_RTOS_H
#define APP_RTOS_H

#include <stdint.h>

/* Stack high-water marks are in 32-bit words, updated by each task. */
typedef struct
{
    volatile uint32_t input_cycles;
    volatile uint32_t control_cycles;
    volatile uint32_t display_cycles;
    volatile uint32_t input_stack_free;
    volatile uint32_t control_stack_free;
    volatile uint32_t display_stack_free;
    volatile uint32_t queue_overflows;
} AppDiagnostics;

extern AppDiagnostics app_diagnostics;
void AppRTOS_Start(void);

#endif
