#ifndef APP_H
#define APP_H

#include <stdint.h>
#include "dht11.h"
#include "relay_test_config.h"

typedef enum
{
    APP_STANDBY,
    APP_SETTING,
    APP_HEATING,
    APP_PAUSED,
    APP_COMPLETED
} AppState;

typedef enum
{
    PAGE_WELCOME,
    PAGE_HOME,
    PAGE_TIME,
    PAGE_POWER,
    PAGE_RUNNING,
    PAGE_PAUSED,
    PAGE_COMPLETED,
#if RELAY_PULSE_TEST
    PAGE_RELAY_TEST,
#endif
    PAGE_COUNT
} UIPage;

typedef enum
{
    NOTICE_NONE,
    NOTICE_CLOSE_DOOR,
    NOTICE_SET_TIME,
    NOTICE_RUNNING,
    NOTICE_PAUSE_FIRST,
    NOTICE_RESUME,
    NOTICE_ENTER_TIME
} AppNotice;

typedef struct
{
    uint8_t state;
    uint8_t page;
    uint8_t door_closed;          /* KEY1 门舵机命令 */
#if RELAY_LED_COUNTDOWN_TEST
    uint8_t door_feedback_closed; /* PB10 消抖后的实际反馈 */
#endif
    uint8_t power_level;
    uint16_t cook_seconds;
    uint16_t display_seconds;
    uint32_t remaining_ms;
    uint32_t total_ms;
    uint8_t notice;
    Dht11Reading environment;
#if RELAY_PULSE_TEST
    uint8_t relay_active;
#endif
#if PTC_BENCH_TEST
    uint8_t relay_test_finished;
#endif
} AppSnapshot;

void App_Init(uint32_t now);
void App_Update(uint32_t now);
#if RELAY_LED_COUNTDOWN_TEST
void App_SetDoorFeedback(uint8_t closed);
#endif
void App_HandleKeys(uint8_t key1, uint8_t key2, uint32_t now);
void App_GetSnapshot(AppSnapshot *snapshot);

#endif
