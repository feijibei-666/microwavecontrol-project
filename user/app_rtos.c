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
#include "outputs.h"
#include "buzzer.h"
#if RELAY_PULSE_TEST
#include "heater.h"
#elif RELAY_LED_COUNTDOWN_TEST
#include "door_switch.h"
#endif

#define INPUT_STACK_WORDS  192
#define CONTROL_STACK_WORDS 384
#define DISPLAY_STACK_WORDS 512
#define BUZZER_STACK_WORDS 192
#define SENSOR_STACK_WORDS 192
#define SENSOR_PERIOD_MS   5000U
#define SENSOR_STALE_MS    (2U * SENSOR_PERIOD_MS)
#define INPUT_QUEUE_LENGTH 12

typedef struct
{
    uint8_t key1;
    uint8_t key2;
#if RELAY_LED_COUNTDOWN_TEST
    uint8_t door_feedback_closed;
#endif
} InputEvent;

typedef struct
{
    Dht11Reading reading;
    uint32_t sampled_at;
} SensorSample;

static StaticTask_t input_tcb, control_tcb, display_tcb;
static StaticTask_t buzzer_tcb;
static StaticTask_t sensor_tcb;
static StackType_t input_stack[INPUT_STACK_WORDS];
static StackType_t control_stack[CONTROL_STACK_WORDS];
static StackType_t display_stack[DISPLAY_STACK_WORDS];
static StackType_t buzzer_stack[BUZZER_STACK_WORDS];
static StackType_t sensor_stack[SENSOR_STACK_WORDS];
static StaticQueue_t input_queue_control, snapshot_queue_control;
static StaticQueue_t buzzer_queue_control;
static StaticQueue_t sensor_queue_control;
static uint8_t input_queue_storage[INPUT_QUEUE_LENGTH * sizeof(InputEvent)];
static uint8_t snapshot_queue_storage[sizeof(AppSnapshot)];
static uint8_t buzzer_queue_storage[sizeof(uint8_t)];
static uint8_t sensor_queue_storage[sizeof(SensorSample)];
static QueueHandle_t input_queue, snapshot_queue;
static QueueHandle_t buzzer_queue;
static QueueHandle_t sensor_queue;
static volatile uint8_t display_ready;
AppDiagnostics app_diagnostics;

static void InputTask(void *argument)
{
    TickType_t wake = xTaskGetTickCount();
    InputEvent event;
#if RELAY_LED_COUNTDOWN_TEST
    uint8_t door_changed;
    uint8_t previous_door_closed = 0;
    uint8_t key1_held_at_close = 0;
#endif
    (void)argument;
    for (;;)
    {
#if RELAY_LED_COUNTDOWN_TEST
        event.door_feedback_closed = DoorSwitch_Sample(delay_millis());
        door_changed = (uint8_t)(event.door_feedback_closed != previous_door_closed);
        previous_door_closed = event.door_feedback_closed;
        if (door_changed && event.door_feedback_closed)
        {
            key1_held_at_close = (uint8_t)(KEY1_READ() == KEY_ACTIVE_LEVEL);
        }
#endif
        event.key1 = KEY_NONE;
        event.key2 = KEY_NONE;
        if (display_ready)
        {
            event.key1 = KEY1_Scan();
            event.key2 = KEY2_Scan();
#if RELAY_LED_COUNTDOWN_TEST
            /* 关门前就按住 KEY1 时，本次长按不能启动或恢复。 */
            if (key1_held_at_close && event.key1 == KEY_LONG) { event.key1 = KEY_NONE; }
#endif
        }
#if RELAY_LED_COUNTDOWN_TEST
        if (KEY1_READ() != KEY_ACTIVE_LEVEL) { key1_held_at_close = 0; }
        /* 门事件不依赖 LCD 就绪，与同拍按键一起送 Control 优先处理。 */
        if (door_changed || event.key1 != KEY_NONE || event.key2 != KEY_NONE)
#else
        if (event.key1 != KEY_NONE || event.key2 != KEY_NONE)
#endif
        {
            if (xQueueSend(input_queue, &event, 0) != pdPASS)
            {
                app_diagnostics.queue_overflows++;
                RTOS_Fatal(4);
            }
        }
        app_diagnostics.input_cycles++;
        app_diagnostics.input_stack_free = uxTaskGetStackHighWaterMark(NULL);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(1));
    }
}

#if RELAY_PULSE_TEST
static void ControlTask(void *argument)
{
    AppSnapshot snapshot = {0};
    InputEvent event;
    TickType_t energized_at = 0;
    TickType_t now;
    BaseType_t received;
    (void)argument;
    snapshot.page = PAGE_RELAY_TEST;

    for (;;)
    {
        /* 空闲时等待按键。吸合时每 10 ms 醒来检查截止时间，不阻塞其他任务。 */
        received = xQueueReceive(input_queue, &event,
                                 snapshot.relay_active ? pdMS_TO_TICKS(10) : portMAX_DELAY);
        now = xTaskGetTickCount();
        if (snapshot.relay_active)
        {
#if PTC_BENCH_TEST
            /* 有负载的试热中，KEY2 短按或长按直接取消输出。 */
            if (!(received == pdPASS && event.key2 != KEY_NONE) &&
                (TickType_t)(now - energized_at) < pdMS_TO_TICKS(RELAY_TEST_PULSE_MS))
#else
            /* 吸合期间忽略新的按键，包括截止这一轮收到的按键，避免延长脉冲。 */
            if ((TickType_t)(now - energized_at) < pdMS_TO_TICKS(RELAY_TEST_PULSE_MS))
#endif
            {
                continue;
            }
            Heater_Off();
            snapshot.relay_active = 0;
#if PTC_BENCH_TEST
            snapshot.relay_test_finished = 1;
#endif
        }
        else if (received == pdPASS && event.key1 == KEY_LONG
#if PTC_BENCH_TEST
                 && event.key2 == KEY_NONE && !snapshot.relay_test_finished
#endif
                )
        {
            /* 消抖器每次长按只发一个事件；松开再按才会触发下一次。 */
            Heater_TestEnergize();
            energized_at = xTaskGetTickCount();
            snapshot.relay_active = 1;
        }
        else
        {
            continue;
        }

        /* 先更新 PA2，再通知显示任务。LCD 只显示指令，不能测量触点状态。 */
        configASSERT(xQueueOverwrite(snapshot_queue, &snapshot) == pdPASS);
        app_diagnostics.control_cycles++;
        app_diagnostics.control_stack_free = uxTaskGetStackHighWaterMark(NULL);
    }
}
#else
static void Control_ApplyEffects(AppSnapshot *previous, AppSnapshot *current,
                                 uint8_t user_event)
{
    uint8_t pulses = 0;
    uint8_t running = (uint8_t)(current->state == APP_HEATING);
    uint8_t relay_requested = 0;

    /* 输出先于提示音和绘屏执行；开门路径由 BSP 保证先停转、断加热。 */
#if RELAY_LED_COUNTDOWN_TEST
    static const uint16_t on_ms[] = {10000U, 14000U, 20000U};
    uint32_t elapsed_ms = current->total_ms - current->remaining_ms;

    running = (uint8_t)(running && current->door_feedback_closed);
    /* 按累计运行时间计相位，暂停期间窗口冻结。仅在有效档位请求吸合。 */
    relay_requested = (uint8_t)(running && current->remaining_ms > 0U &&
        current->power_level < sizeof(on_ms) / sizeof(on_ms[0]) &&
        elapsed_ms % 20000U < on_ms[current->power_level]);
    current->relay_active = Outputs_Apply(current->door_closed, running, relay_requested);
#else
    (void)Outputs_Apply(current->door_closed, running, relay_requested);
#endif
    if (current->state != previous->state)
    {
        if (current->state == APP_HEATING) { pulses = 2; }
        else if (current->state == APP_COMPLETED) { pulses = 3; }
        else if (current->state == APP_PAUSED) { pulses = 1; }
        else if (current->state == APP_STOPPED) { pulses = 4; }
    }
    if (pulses == 0 && user_event && current->notice != NOTICE_NONE)
    {
        /* 每次被拒绝的操作提示一次，不因同一提示持续显示而反复鸣叫。 */
        pulses = 1;
    }
    if (pulses != 0)
    {
        /* 最新提示覆盖尚未播放的旧提示；控制任务绝不等待鸣叫完成。 */
        configASSERT(xQueueOverwrite(buzzer_queue, &pulses) == pdPASS);
    }
    *previous = *current;
}

static void ControlTask(void *argument)
{
    InputEvent event;
    AppSnapshot snapshot, previous;
    BaseType_t received;
    uint32_t now;
    Dht11Reading effective_reading;
    uint8_t just_stopped;
    SensorSample sample = {{0, 0, DHT11_WAITING}, 0};
    (void)argument;
    App_GetSnapshot(&previous);
    for (;;)
    {
        received = xQueueReceive(input_queue, &event, pdMS_TO_TICKS(10));
        now = delay_millis();
        /* 温度保护先于计时和按键。同轮的长按不能绕开过温或读数超时。 */
        (void)xQueueReceive(sensor_queue, &sample, 0);
        effective_reading = sample.reading;
        if ((uint32_t)(now - sample.sampled_at) >= SENSOR_STALE_MS &&
            effective_reading.status == DHT11_OK)
        {
            effective_reading.status = DHT11_TIMEOUT;
        }
        App_SetTemperature(effective_reading);
#if RELAY_LED_COUNTDOWN_TEST
        if (received == pdPASS)
        {
            /* 先处理开门，再走计时和输出。按键无法抢在同拍门反馈之前启动。 */
            App_SetDoorFeedback(event.door_feedback_closed);
        }
#endif
        App_Update(now);
        App_GetSnapshot(&snapshot);
        just_stopped = (uint8_t)(snapshot.state == APP_STOPPED && previous.state != APP_STOPPED);
        Control_ApplyEffects(&previous, &snapshot, 0);
        if (!just_stopped && received == pdPASS &&
            (event.key1 != KEY_NONE || event.key2 != KEY_NONE))
        {
            /* 先处理本轮到期停机，再处理按键，避免同次输入跳过安全输出。 */
            App_HandleKeys(event.key1, event.key2, now);
            App_GetSnapshot(&snapshot);
            Control_ApplyEffects(&previous, &snapshot, 1);
        }
        /* A one-element mailbox keeps the newest state during transitions. */
        configASSERT(xQueueOverwrite(snapshot_queue, &snapshot) == pdPASS);
        app_diagnostics.control_cycles++;
        app_diagnostics.control_stack_free = uxTaskGetStackHighWaterMark(NULL);
    }
}

#endif

static void SensorTask(void *argument)
{
    TickType_t wake = xTaskGetTickCount();
    SensorSample sample = {{0, 0, DHT11_WAITING}, 0};
    (void)argument;
    Dht11_Init();
    /* 给上电后的传感器留出稳定时间。TIM1 捕获边沿，任务只等采样周期。 */
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    for (;;)
    {
        sample.reading = Dht11_Read();
        sample.sampled_at = delay_millis();
        configASSERT(xQueueOverwrite(sensor_queue, &sample) == pdPASS);
        app_diagnostics.sensor_status = sample.reading.status;
        app_diagnostics.sensor_cycles++;
        app_diagnostics.sensor_stack_free = uxTaskGetStackHighWaterMark(NULL);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}

static void BuzzerTask(void *argument)
{
    uint8_t pulses;
    TickType_t wait_ticks;
    uint32_t now;
    (void)argument;
    for (;;)
    {
        /* 空闲时阻塞等命令；播放时以 10 ms 步长推进响/静音状态机。 */
        wait_ticks = Buzzer_IsBusy() ? pdMS_TO_TICKS(10) : portMAX_DELAY;
        if (xQueueReceive(buzzer_queue, &pulses, wait_ticks) == pdPASS)
        {
            now = delay_millis();
            Buzzer_Start(pulses, now);
        }
        Buzzer_Update(delay_millis());
        app_diagnostics.buzzer_cycles++;
        app_diagnostics.buzzer_stack_free = uxTaskGetStackHighWaterMark(NULL);
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
    AppSnapshot snapshot = {0};
    display_ready = 0;
#if RELAY_PULSE_TEST
    snapshot.page = PAGE_RELAY_TEST;
#else
#if RELAY_LED_COUNTDOWN_TEST
    DoorSwitch_Init();
#endif
    App_Init(0);
    App_GetSnapshot(&snapshot);
#endif
    input_queue = xQueueCreateStatic(INPUT_QUEUE_LENGTH, sizeof(InputEvent),
                                    input_queue_storage, &input_queue_control);
    snapshot_queue = xQueueCreateStatic(1, sizeof(AppSnapshot),
                                       snapshot_queue_storage, &snapshot_queue_control);
    buzzer_queue = xQueueCreateStatic(1, sizeof(uint8_t),
                                     buzzer_queue_storage, &buzzer_queue_control);
    sensor_queue = xQueueCreateStatic(1, sizeof(SensorSample),
                                     sensor_queue_storage, &sensor_queue_control);
    if (input_queue == NULL || snapshot_queue == NULL || buzzer_queue == NULL ||
        sensor_queue == NULL)
    { RTOS_Fatal(1); }
    configASSERT(xQueueOverwrite(snapshot_queue, &snapshot) == pdPASS);
    if (xTaskCreateStatic(InputTask, "Input", INPUT_STACK_WORDS, NULL, 3,
                          input_stack, &input_tcb) == NULL ||
        xTaskCreateStatic(ControlTask, "Control", CONTROL_STACK_WORDS, NULL, 4,
                          control_stack, &control_tcb) == NULL ||
        xTaskCreateStatic(BuzzerTask, "Buzzer", BUZZER_STACK_WORDS, NULL, 2,
                          buzzer_stack, &buzzer_tcb) == NULL ||
        xTaskCreateStatic(DisplayTask, "Display", DISPLAY_STACK_WORDS, NULL, 1,
                          display_stack, &display_tcb) == NULL ||
        xTaskCreateStatic(SensorTask, "Sensor", SENSOR_STACK_WORDS, NULL, 1,
                          sensor_stack, &sensor_tcb) == NULL)
    { RTOS_Fatal(2); }
    vTaskStartScheduler();
    RTOS_Fatal(3);
}
