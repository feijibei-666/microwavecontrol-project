#include "outputs.h"
#include "heater.h"
#include "servo.h"
#include "turntable.h"

static volatile uint8_t outputs_ready = 0;
static volatile uint8_t outputs_faulted = 0;
static uint8_t last_door_closed = 0;
static uint8_t last_running = 0;

void Outputs_Init(void)
{
    outputs_ready = 0;
    /* 先关闭继电器并给转盘停止脉冲，再初始化门，避免启动时负载先于保护动作。 */
    Heater_Init();
    if (outputs_faulted)
    {
        /* 故障锁存不能通过重复初始化解除，也不能重新移动门。 */
        Turntable_EmergencyStop();
        return;
    }
    Turntable_Init();
    Servo_Init();
    last_door_closed = 0;
    last_running = 0;
    outputs_ready = 1;
}

void Outputs_Apply(uint8_t door_closed, uint8_t running)
{
    if (!outputs_ready || outputs_faulted)
    {
        Heater_Off();
        Turntable_EmergencyStop();
        return;
    }
    door_closed = (uint8_t)(door_closed != 0);
    running = (uint8_t)(running != 0 && door_closed);

#if RELAY_LED_COUNTDOWN_TEST
    /* 联调只驱动 LED 测试回路。先处理 PA2，再处理舵机，运行中保持电平。 */
    if (running) { Heater_TestEnergize(); }
    else { Heater_Off(); }
#else
    /* 常规固件维持关闭，功率档位仍只用于界面演示。 */
    Heater_Off();
#endif

    /* 开门、暂停或完成的停转指令先于门舵机位置更新。 */
    if (!running)
    {
        Turntable_Stop();
    }
    if (door_closed != last_door_closed)
    {
        Servo_SetAngle(door_closed ? SERVO_ANGLE_CLOSE : SERVO_ANGLE_OPEN);
        last_door_closed = door_closed;
    }
    if (running && !last_running)
    {
        Turntable_Start();
    }
    last_running = running;
}

void Outputs_EmergencyStop(void)
{
    /* 不移动门，不等待队列；锁存后只有复位才能重新允许正常执行机构动作。 */
    outputs_faulted = 1;
    Heater_Off();
    Turntable_EmergencyStop();
}
