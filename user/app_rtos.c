#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "app.h"
#include "app_rtos.h"
#include "ui.h"
#include "lcd.h"
#include "key.h"
#include "delay.h"
#include "rtos_hooks.h"

#define INPUT_STACK_WORDS  192
#define CONTROL_STACK_WORDS 256
#define DISPLAY_STACK_WORDS 512
#define INPUT_QUEUE_LENGTH 12

typedef struct
{
    uint8_t key1;
    uint8_t key2;
} InputEvent;

static StaticTask_t input_tcb, control_tcb, display_tcb;
static StackType_t input_stack[INPUT_STACK_WORDS];
static StackType_t control_stack[CONTROL_STACK_WORDS];
static StackType_t display_stack[DISPLAY_STACK_WORDS];
static StaticQueue_t input_queue_control, snapshot_queue_control;
static uint8_t input_queue_storage[INPUT_QUEUE_LENGTH * sizeof(InputEvent)];
static uint8_t snapshot_queue_storage[sizeof(AppSnapshot)];
static QueueHandle_t input_queue, snapshot_queue;
static volatile uint8_t display_ready;
AppDiagnostics app_diagnostics;

static void InputTask(void *argument)
{
    TickType_t wake = xTaskGetTickCount();
    InputEvent event;
    (void)argument;
    for (;;)
    {
        if (display_ready)
        {
            event.key1 = KEY1_Scan();
            event.key2 = KEY2_Scan();
            if (event.key1 != KEY_NONE || event.key2 != KEY_NONE)
            {
                if (xQueueSend(input_queue, &event, 0) != pdPASS)
                {
                    app_diagnostics.queue_overflows++;
                    RTOS_Fatal(4);
                }
            }
        }
        app_diagnostics.input_cycles++;
        app_diagnostics.input_stack_free = uxTaskGetStackHighWaterMark(NULL);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(1));
    }
}

static void ControlTask(void *argument)
{
    InputEvent event;
    AppSnapshot snapshot;
    BaseType_t received;
    uint32_t now;
    (void)argument;
    for (;;)
    {
        received = xQueueReceive(input_queue, &event, pdMS_TO_TICKS(10));
        now = delay_millis();
        App_Update(now);
        if (received == pdPASS) { App_HandleKeys(event.key1, event.key2, now); }
        App_GetSnapshot(&snapshot);
        /* A one-element mailbox keeps the newest state during transitions. */
        configASSERT(xQueueOverwrite(snapshot_queue, &snapshot) == pdPASS);
        app_diagnostics.control_cycles++;
        app_diagnostics.control_stack_free = uxTaskGetStackHighWaterMark(NULL);
    }
}

static void DisplayTask(void *argument)
{
    TickType_t wake;
    AppSnapshot snapshot;
    uint32_t now;
    (void)argument;
    /* Only this task ever accesses SPI1, DMA1 channel 3 and the display. */
    LCD_Init();
    if (lcd_fault_code != 0) { RTOS_Fatal(5); }
    UI_Init();
    KEY_ClearEvents();
    display_ready = 1;
    wake = xTaskGetTickCount();
    for (;;)
    {
        now = delay_millis();
        if (xQueueReceive(snapshot_queue, &snapshot, 0) == pdPASS)
        {
            UI_SetSnapshot(&snapshot, now);
        }
        UI_RenderStep(now);
        if (lcd_fault_code != 0) { RTOS_Fatal(5); }
        app_diagnostics.display_cycles++;
        app_diagnostics.display_stack_free = uxTaskGetStackHighWaterMark(NULL);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(10));
    }
}

void AppRTOS_Start(void)
{
    AppSnapshot snapshot;
    display_ready = 0;
    App_Init(0);
    input_queue = xQueueCreateStatic(INPUT_QUEUE_LENGTH, sizeof(InputEvent),
                                    input_queue_storage, &input_queue_control);
    snapshot_queue = xQueueCreateStatic(1, sizeof(AppSnapshot),
                                       snapshot_queue_storage, &snapshot_queue_control);
    if (input_queue == NULL || snapshot_queue == NULL) { RTOS_Fatal(1); }
    App_GetSnapshot(&snapshot);
    configASSERT(xQueueOverwrite(snapshot_queue, &snapshot) == pdPASS);
    if (xTaskCreateStatic(InputTask, "Input", INPUT_STACK_WORDS, NULL, 3,
                          input_stack, &input_tcb) == NULL ||
        xTaskCreateStatic(ControlTask, "Control", CONTROL_STACK_WORDS, NULL, 2,
                          control_stack, &control_tcb) == NULL ||
        xTaskCreateStatic(DisplayTask, "Display", DISPLAY_STACK_WORDS, NULL, 1,
                          display_stack, &display_tcb) == NULL)
    { RTOS_Fatal(2); }
    vTaskStartScheduler();
    RTOS_Fatal(3);
}
