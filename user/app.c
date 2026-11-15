#include "app.h"
#include "key.h"
#include "led.h"

#define TIME_STEP 20U
#define TIME_MAX 200U
#define NOTICE_MS 1800U
#define WELCOME_MS 2000U
#define OVER_TEMP_C 30U

typedef struct
{
    AppSnapshot view;
    uint32_t last_tick;
    uint32_t notice_start;
    uint32_t welcome_start;
} MicrowaveApp;

static MicrowaveApp app;

static void App_SetDoor(uint8_t closed)
{
    app.view.door_closed = closed;
#if !RELAY_LED_COUNTDOWN_TEST
    if (closed) { LED_OFF(); }
    else { LED_ON(); }
#endif
}

static void App_Pause(void)
{
    app.view.state = APP_PAUSED;
    app.view.page = PAGE_PAUSED;
    app.view.notice = NOTICE_NONE;
}

static void App_Stop(AppStopReason reason)
{
    app.view.cook_seconds = 0;
    app.view.remaining_ms = 0;
    app.view.total_ms = 0;
    app.view.power_level = 1;
    App_SetDoor(0);
    app.view.state = APP_STOPPED;
    app.view.page = PAGE_STOPPED;
    app.view.notice = NOTICE_NONE;
    app.view.stop_reason = (uint8_t)reason;
}

static AppStopReason App_TemperatureFault(void)
{
    if (app.view.environment.status != DHT11_OK) { return STOP_TEMP_NO_DATA; }
    return app.view.environment.temperature_c >= OVER_TEMP_C ? STOP_OVER_TEMP : STOP_NONE;
}

void App_SetTemperature(Dht11Reading reading)
{
    AppStopReason fault;
    app.view.environment = reading;
    if (app.view.state == APP_HEATING || app.view.state == APP_PAUSED)
    {
        /* 高温或读数失效会废弃整轮，不能在重新关门后继续旧倒计时。 */
        fault = App_TemperatureFault();
        if (fault != STOP_NONE) { App_Stop(fault); }
    }
}

#if RELAY_LED_COUNTDOWN_TEST
void App_SetDoorFeedback(uint8_t closed)
{
    app.view.door_feedback_closed = closed;
    if (closed) { LED_OFF(); }
    else
    {
        LED_ON();
        /* 开门反馈保留剩余时间。反馈恢复闭合后仍等待显式恢复键。 */
        if (app.view.state == APP_HEATING) { App_Pause(); }
    }
}
#endif

void App_Init(uint32_t now)
{
    app.view.state = APP_STANDBY;
    app.view.page = PAGE_WELCOME;
    App_SetDoor(0);
#if RELAY_LED_COUNTDOWN_TEST
    App_SetDoorFeedback(0);
#endif
    app.view.power_level = 1;
    app.view.cook_seconds = 0;
    app.view.display_seconds = 0;
    app.view.remaining_ms = 0;
    app.view.total_ms = 0;
    app.view.notice = NOTICE_NONE;
    app.view.stop_reason = STOP_NONE;
    app.view.environment = (Dht11Reading){0, 0, DHT11_WAITING};
    app.last_tick = now;
    app.notice_start = now;
    app.welcome_start = now;
}

static void App_Notice(AppNotice notice, uint32_t now)
{
    app.view.notice = (uint8_t)notice;
    app.notice_start = now;
}

void App_Update(uint32_t now)
{
    uint32_t elapsed = (uint32_t)(now - app.last_tick);
    app.last_tick = now;
    if (app.view.page == PAGE_WELCOME && (uint32_t)(now - app.welcome_start) >= WELCOME_MS)
    {
        app.view.page = PAGE_IDLE;
    }
    if (app.view.notice != NOTICE_NONE &&
        (uint32_t)(now - app.notice_start) >= NOTICE_MS)
    {
        app.view.notice = NOTICE_NONE;
    }
    if (app.view.state == APP_HEATING)
    {
        if (elapsed >= app.view.remaining_ms)
        {
            app.view.remaining_ms = 0;
            app.view.cook_seconds = 0;
            app.view.power_level = 1;
            App_SetDoor(0);
            app.view.state = APP_COMPLETED;
            app.view.page = PAGE_COMPLETED;
            app.view.notice = NOTICE_NONE;
        }
        else { app.view.remaining_ms -= elapsed; }
    }
}

static void App_Key1(uint8_t event, uint32_t now)
{
    AppStopReason fault;

    if (event == KEY_SHORT)
    {
        app.view.notice = NOTICE_NONE;
        app.view.stop_reason = STOP_NONE;
        App_SetDoor((uint8_t)!app.view.door_closed);
        if (app.view.state == APP_HEATING)
        {
            /* 短按开门也可主动暂停，关断不等待门舵机到位。 */
            App_Pause();
        }
        else if (app.view.state == APP_PAUSED)
        {
            app.view.page = PAGE_PAUSED;
        }
        else if (app.view.door_closed)
        {
            app.view.state = APP_SETTING;
            app.view.page = PAGE_HOME;
        }
        else
        {
            app.view.state = APP_STANDBY;
            app.view.page = PAGE_IDLE;
        }
    }
    else if (event == KEY_LONG)
    {
        /* 停机页保留原因，短按 KEY1 才进入新一轮设置。 */
        if (app.view.state == APP_STOPPED) { return; }
        fault = App_TemperatureFault();
        if (!app.view.door_closed
#if RELAY_LED_COUNTDOWN_TEST
            || !app.view.door_feedback_closed
#endif
           )
        { App_Notice(NOTICE_CLOSE_DOOR, now); }
        else if (app.view.state == APP_HEATING) { App_Notice(NOTICE_RUNNING, now); }
        else if (app.view.state != APP_PAUSED && app.view.cook_seconds == 0)
        { App_Notice(NOTICE_SET_TIME, now); }
        else if (fault != STOP_NONE)
        {
            App_Notice(fault == STOP_OVER_TEMP ? NOTICE_OVER_TEMP : NOTICE_TEMP_NO_DATA, now);
        }
        else
        {
            if (app.view.state != APP_PAUSED)
            {
                app.view.remaining_ms = (uint32_t)app.view.cook_seconds * 1000U;
                app.view.total_ms = app.view.remaining_ms;
            }
            app.view.state = APP_HEATING;
            app.view.page = PAGE_RUNNING;
            app.last_tick = now;
            app.view.notice = NOTICE_NONE;
        }
    }
}

static void App_Key2(uint8_t event, uint32_t now)
{
    if (event == KEY_NONE || app.view.state == APP_STOPPED) { return; }
#if RELAY_LED_COUNTDOWN_TEST
    if (event == KEY_LONG && app.view.state == APP_HEATING)
    {
        /* 保留低压联调的手动停机入口，便于不升温时调试输出链。 */
        App_Stop(STOP_TEST);
        return;
    }
#endif
    if (!app.view.door_closed) { App_Notice(NOTICE_CLOSE_DOOR, now); return; }
    /* 进入设置后可继续编辑参数，物理门反馈只限制启动和恢复。 */
    if (app.view.state == APP_HEATING) { App_Notice(NOTICE_PAUSE_FIRST, now); return; }
    if (app.view.state == APP_PAUSED) { App_Notice(NOTICE_RESUME, now); return; }
    app.view.notice = NOTICE_NONE;
    if (app.view.page == PAGE_HOME)
    {
        if (event == KEY_SHORT) { app.view.page = PAGE_TIME; }
        else { App_Notice(NOTICE_ENTER_TIME, now); }
    }
    else if (event == KEY_LONG)
    {
        app.view.page = (app.view.page == PAGE_TIME) ? PAGE_POWER : PAGE_TIME;
    }
    else if (event == KEY_SHORT && app.view.page == PAGE_TIME)
    {
        app.view.cook_seconds += TIME_STEP;
        if (app.view.cook_seconds > TIME_MAX) { app.view.cook_seconds = 0; }
    }
    else if (event == KEY_SHORT && app.view.page == PAGE_POWER)
    {
        app.view.power_level = (uint8_t)((app.view.power_level + 1U) % 3U);
    }
}

void App_HandleKeys(uint8_t key1, uint8_t key2, uint32_t now)
{
    /* Door events take precedence when both keys are delivered together. */
    App_Key1(key1, now);
    if (key1 == KEY_NONE) { App_Key2(key2, now); }
}

void App_GetSnapshot(AppSnapshot *snapshot)
{
    *snapshot = app.view;
    if (app.view.state == APP_HEATING || app.view.state == APP_PAUSED)
    {
        snapshot->display_seconds = (uint16_t)((app.view.remaining_ms + 999U) / 1000U);
    }
    else { snapshot->display_seconds = app.view.cook_seconds; }
}
