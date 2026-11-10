#include "stm32f10x.h"
#include "delay.h"
#include "led.h"
#include "key.h"
#include "servo.h"
#include "turntable.h"
#include "heater.h"
#include "lcd.h"

/* 锟斤拷锟斤拷状态 */
#define MODE_NONE   0       /* 未锟斤拷锟斤拷锟斤拷锟矫ｏ拷只锟斤拷示默锟斤拷时锟斤拷凸锟斤拷锟?*/
#define MODE_TIME   1       /* 锟斤拷锟斤拷时锟斤拷锟斤拷 */
#define MODE_POWER  2       /* 锟斤拷锟绞碉拷位锟斤拷锟斤拷 */

#define TIME_STEP   20      /* 锟教帮拷一锟轿硷拷 20s */
#define TIME_MAX    200     /* 锟斤拷锟斤拷时锟斤拷锟街?*/

static const char *POWER_NAME[3] = {"Low", "Medium", "High"};

static uint16_t cook_seconds = 0;       /* 锟斤拷锟斤拷时锟斤拷默锟斤拷 0s */
static uint8_t  power_level  = 1;       /* 0=Low 1=Medium 2=High锟斤拷默锟斤拷 Medium */
static uint8_t  setting_mode = MODE_NONE;

/* 锟睫凤拷锟斤拷锟斤拷锟斤拷转锟街凤拷锟斤拷锟斤拷buf 锟斤拷锟斤拷 6 锟街斤拷 */
static void U16ToStr(uint16_t value, char *buf)
{
    char    tmp[6];
    uint8_t n = 0;
    uint8_t i;

    if (value == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (value > 0)
    {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    }
    for (i = 0; i < n; i++)
    {
        buf[i] = tmp[n - 1 - i];
    }
    buf[n] = '\0';
}

/* 刷锟斤拷锟斤拷锟矫斤拷锟芥：锟斤拷锟斤拷 + 锟斤拷锟斤拷时 + 锟斤拷锟绞碉拷位 */
static void ShowSettingScreen(void)
{
    char num[6];

    LCD_Clear(LCD_BLACK);

    if (setting_mode == MODE_TIME)
    {
        LCD_ShowStringCenter(16, "time setting", LCD_YELLOW, LCD_BLACK);
    }
    else if (setting_mode == MODE_POWER)
    {
        LCD_ShowStringCenter(16, "Power setting", LCD_YELLOW, LCD_BLACK);
    }

    U16ToStr(cook_seconds, num);
    LCD_ShowString(0, 64, "Time:", LCD_WHITE, LCD_BLACK);
    LCD_ShowString(48, 64, num, LCD_WHITE, LCD_BLACK);
    LCD_ShowString(80, 64, "s", LCD_WHITE, LCD_BLACK);

    LCD_ShowString(0, 96, "Power:", LCD_WHITE, LCD_BLACK);
    LCD_ShowString(56, 96, POWER_NAME[power_level], LCD_CYAN, LCD_BLACK);
}

int main(void)
{
    uint8_t door_closed = 0;
    uint8_t running     = 0;            /* 0: stopped  1: heating+turntable running */           /* 0锟斤拷锟斤拷未锟截ｏ拷锟斤拷锟斤拷锟斤拷  1锟斤拷锟斤拷锟窖癸拷 */
    uint8_t  key;

    delay_init();
    LED_Init();
    KEY_Init();
    Servo_Init();
    Turntable_Init();
    Heater_Init();
    LCD_Init();

    /* 锟斤拷锟斤拷锟斤拷迎锟斤拷锟芥，锟斤拷示 2 锟斤拷 */
    LCD_Clear(LCD_BLUE);
    LCD_ShowStringCenter(64, "Microwave Demo", LCD_WHITE, LCD_BLUE);
    LCD_ShowStringCenter(84, "V1.1", LCD_WHITE, LCD_BLUE);
    delay_ms(2000);

    /* 锟斤拷锟斤拷锟斤拷锟斤拷锟角伙拷锟?LED 锟斤拷锟斤拷锟斤拷锟斤拷锟酵ｏ拷诳锟斤拷锟轿伙拷锟?*/
    LCD_Clear(LCD_BLACK);
    LCD_ShowStringCenter(72, "Standby", LCD_GREEN, LCD_BLACK);
    LED_ON();
    Servo_SetAngle(SERVO_ANGLE_OPEN);

    while (1)
    {
        if (door_closed == 0)
        {
            /* 锟斤拷一锟轿帮拷锟斤拷 KEY1 锟斤拷锟脚ｏ拷锟斤拷锟阶?90 锟饺ｏ拷腔锟斤拷锟较拷锟?*/
            if (KEY1_Pressed() == 1)
            {
                Servo_SetAngle(SERVO_ANGLE_CLOSE);
                LED_OFF();
                door_closed = 1;

                setting_mode = MODE_NONE;
                ShowSettingScreen();        /* 默锟较ｏ拷0s / Medium */
            }
        }
        else
        {
            /* long press KEY1: start heating + turntable; long press again: stop */
            if (KEY1_Scan() == KEY_LONG)
            {
                if (running == 0)
                {
                    Heater_SetPower(power_level);
                    Turntable_Start();      /* 1.0ms pulse: spin continuously */
                    running = 1;
                }
                else
                {
                    Heater_Off();
                    Turntable_Stop();       /* 1.5ms pulse: stop */
                    running = 0;
                }
            }

            key = KEY2_Scan();

            if (key == KEY_SHORT)
            {
                if (setting_mode == MODE_NONE)
                {
                    setting_mode = MODE_TIME;       /* 锟阶次短帮拷锟斤拷锟诫倒锟斤拷时锟斤拷锟斤拷 */
                }
                else if (setting_mode == MODE_TIME)
                {
                    cook_seconds += TIME_STEP;      /* 锟教帮拷 +20s锟斤拷锟接碉拷 200s 锟斤拷氐锟?0 */
                    if (cook_seconds > TIME_MAX)
                    {
                        cook_seconds = 0;
                    }
                }
                else
                {
                    power_level++;                  /* Low -> Medium -> High -> Low */
                    if (power_level > 2)
                    {
                        power_level = 0;
                    }
                }
                ShowSettingScreen();
            }
            else if (key == KEY_LONG)
            {
                if (setting_mode == MODE_POWER)
                {
                    setting_mode = MODE_TIME;
                }
                else
                {
                    setting_mode = MODE_POWER;
                }
                ShowSettingScreen();
            }
        }
    }
}

