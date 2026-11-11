#include "FreeRTOS.h"
#include "task.h"
#include "key.h"
#include "led.h"
#include "rtos_hooks.h"
#include "outputs.h"
#include "buzzer.h"

volatile uint32_t rtos_fault_code = 0;
volatile uint32_t rtos_fault_line = 0;
const char * volatile rtos_fault_file = 0;
static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack,
                                   uint32_t *stack_words)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *stack_words = configMINIMAL_STACK_SIZE;
}

void vApplicationTickHook(void)
{
    /* The kernel owns SysTick; this bounded hook keeps both keys independent. */
    KEY_Tick1ms();
}

void RTOS_Fatal(uint32_t code)
{
    __disable_irq();
    /* 故障路径不等待任务调度：先断开继电器并停转，再静音、记录故障。 */
    Outputs_EmergencyStop();
    Buzzer_Stop();
    rtos_fault_code = code;
    LED_ON();
    for (;;)
    {
        __NOP();
    }
}

void RTOS_Assert(const char *file, uint32_t line)
{
    rtos_fault_file = file;
    rtos_fault_line = line;
    RTOS_Fatal(0xA001);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    rtos_fault_file = name;
    RTOS_Fatal(0xA002);
}
