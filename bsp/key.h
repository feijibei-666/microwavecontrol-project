#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

/* KEY1��PB12���������룬����Ϊ�͵�ƽ */
#define KEY1_PORT       GPIOB
#define KEY1_PIN        GPIO_Pin_12
#define KEY1_READ()     GPIO_ReadInputDataBit(KEY1_PORT, KEY1_PIN)

/* KEY2��PB1���������룬����Ϊ�͵�ƽ */
#define KEY2_PORT       GPIOB
#define KEY2_PIN        GPIO_Pin_1
#define KEY2_READ()     GPIO_ReadInputDataBit(KEY2_PORT, KEY2_PIN)

/* �����¼� */
#define KEY_NONE        0
#define KEY_SHORT       1       /* �̰� */
#define KEY_LONG        2       /* ��������סԼ 1 �룩 */

void KEY_Init(void);
uint8_t KEY1_Pressed(void);     /* �����������²��ɿ��󷵻� 1�����򷵻� 0 */
uint8_t KEY1_Scan(void);        /* ���� KEY_NONE / KEY_SHORT / KEY_LONG */
uint8_t KEY2_Scan(void);

#endif
