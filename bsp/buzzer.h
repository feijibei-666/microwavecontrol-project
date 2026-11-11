#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

/* PB15 接高电平触发的有源蜂鸣器模块；输入端必须兼容 3.3 V。
 * 裸蜂鸣器不能由 GPIO 直接供电，须增加匹配的晶体管驱动。 */
#define BUZZER_ON_MS       200U
#define BUZZER_GAP_MS      150U
#define BUZZER_MAX_PULSES  4U

void Buzzer_Init(void);
/* 1～4 声；0 立即停止，超过上限按 4 声处理。新命令打断旧序列。 */
void Buzzer_Start(uint8_t pulses, uint32_t now);
void Buzzer_Update(uint32_t now);
uint8_t Buzzer_IsBusy(void);
/* 不调用 RTOS、不延时；初始化前亦可调用，用于故障关闭。 */
void Buzzer_Stop(void);

#endif
