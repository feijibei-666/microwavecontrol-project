#include "key.h"
#include "delay.h"

#define KEY_LONG_MS     1000    /* ��ס������ʱ���㳤�� */

void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = KEY1_PIN | KEY2_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;      /* �������룬����Ϊ�� */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(KEY1_PORT, &GPIO_InitStructure);
}

/* KEY1�������������²��ɿ��󷵻� 1�����ڹ��ţ� */
uint8_t KEY1_Pressed(void)
{
    if (KEY1_READ() == 0)
    {
        delay_ms(20);
        if (KEY1_READ() == 0)
        {
            while (KEY1_READ() == 0);
            return 1;
        }
    }
    return 0;
}

/* KEY1���̰� / ���������ڹ��ź��������ȣ� */
uint8_t KEY1_Scan(void)
{
    uint16_t hold = 0;

    if (KEY1_READ() == 0)
    {
        delay_ms(20);
        if (KEY1_READ() == 0)
        {
            while (KEY1_READ() == 0)
            {
                delay_ms(50);
                hold += 50;
                if (hold >= KEY_LONG_MS)
                {
                    while (KEY1_READ() == 0);
                    return KEY_LONG;
                }
            }
            return KEY_SHORT;
        }
    }
    return KEY_NONE;
}

/* KEY2���̰� / ���� */
uint8_t KEY2_Scan(void)
{
    uint16_t hold = 0;

    if (KEY2_READ() == 0)
    {
        delay_ms(20);
        if (KEY2_READ() == 0)
        {
            while (KEY2_READ() == 0)
            {
                delay_ms(50);
                hold += 50;
                if (hold >= KEY_LONG_MS)
                {
                    while (KEY2_READ() == 0);
                    return KEY_LONG;
                }
            }
            return KEY_SHORT;
        }
    }
    return KEY_NONE;
}
