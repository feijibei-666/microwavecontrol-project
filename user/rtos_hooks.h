#ifndef RTOS_HOOKS_H
#define RTOS_HOOKS_H

#include <stdint.h>

extern volatile uint32_t rtos_fault_code;
extern volatile uint32_t rtos_fault_line;
extern const char * volatile rtos_fault_file;
void RTOS_Fatal(uint32_t code);
void RTOS_Assert(const char *file, uint32_t line);

#endif
