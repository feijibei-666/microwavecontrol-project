#include "key.h"

#define KEY_COUNT       2
#define KEY_QUEUE_SIZE  8

typedef struct
{
    uint8_t candidate;
    uint8_t stable;
    uint8_t debounce_ms;
    uint8_t armed;
    uint8_t long_sent;
    uint16_t held_ms;
    volatile uint8_t events[KEY_QUEUE_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
} KeyState;

static KeyState keys[KEY_COUNT];
static volatile uint8_t key_ready = 0;

static void Key_Push(KeyState *key, uint8_t event)
{
    uint8_t next = (uint8_t)((key->head + 1) % KEY_QUEUE_SIZE);

    if (next != key->tail)
    {
        key->events[key->head] = event;
        key->head = next;
    }
}

static uint8_t Key_Pop(KeyState *key)
{
    uint8_t event = KEY_NONE;
    uint32_t interrupt_mask = __get_PRIMASK();

    __disable_irq();
    if (key->tail != key->head)
    {
        event = key->events[key->tail];
        key->tail = (uint8_t)((key->tail + 1) % KEY_QUEUE_SIZE);
    }
    __set_PRIMASK(interrupt_mask);
    return event;
}

void KEY_Init(void)
{
    GPIO_InitTypeDef gpio;
    uint8_t i;
    uint8_t pressed;

    key_ready = 0;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = KEY1_PIN | KEY2_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(KEY1_PORT, &gpio);

    for (i = 0; i < KEY_COUNT; i++)
    {
        pressed = (uint8_t)(((i == 0) ? KEY1_READ() : KEY2_READ()) == KEY_ACTIVE_LEVEL);
        keys[i].candidate = pressed;
        keys[i].stable = pressed;
        keys[i].debounce_ms = 0;
        /* A key held during reset must be released before it can emit events. */
        keys[i].armed = (uint8_t)!pressed;
        keys[i].long_sent = 0;
        keys[i].held_ms = 0;
        keys[i].head = 0;
        keys[i].tail = 0;
    }
    key_ready = 1;
}

static void Key_Sample(KeyState *key, uint8_t pressed)
{
    if (pressed != key->candidate)
    {
        key->candidate = pressed;
        key->debounce_ms = 1;
    }
    else if (key->debounce_ms < KEY_DEBOUNCE_MS)
    {
        key->debounce_ms++;
    }

    if ((key->debounce_ms >= KEY_DEBOUNCE_MS) && (key->stable != pressed))
    {
        key->stable = pressed;
        if (pressed)
        {
            key->held_ms = 0;
            key->long_sent = 0;
        }
        else
        {
            if (key->armed && !key->long_sent)
            {
                Key_Push(key, KEY_SHORT);
            }
            key->armed = 1;
        }
    }

    /* Also rearm when a startup press was cleared before it became stable. */
    if (!key->stable && !pressed && (key->debounce_ms >= KEY_DEBOUNCE_MS))
    {
        key->armed = 1;
    }

    if (key->stable && pressed && key->armed && !key->long_sent)
    {
        if (key->held_ms < KEY_LONG_MS)
        {
            key->held_ms++;
        }
        if (key->held_ms >= KEY_LONG_MS)
        {
            Key_Push(key, KEY_LONG);
            key->long_sent = 1;
        }
    }
}

void KEY_Tick1ms(void)
{
    if (key_ready)
    {
        Key_Sample(&keys[0], (uint8_t)(KEY1_READ() == KEY_ACTIVE_LEVEL));
        Key_Sample(&keys[1], (uint8_t)(KEY2_READ() == KEY_ACTIVE_LEVEL));
    }
}

void KEY_ClearEvents(void)
{
    uint8_t i;
    uint32_t interrupt_mask = __get_PRIMASK();

    __disable_irq();
    for (i = 0; i < KEY_COUNT; i++)
    {
        keys[i].tail = keys[i].head;
        if (keys[i].stable || keys[i].candidate)
        {
            keys[i].armed = 0;
        }
    }
    __set_PRIMASK(interrupt_mask);
}

uint8_t KEY1_Pressed(void)
{
    return (uint8_t)(KEY1_Scan() == KEY_SHORT);
}

uint8_t KEY1_Scan(void)
{
    return Key_Pop(&keys[0]);
}

uint8_t KEY2_Scan(void)
{
    return Key_Pop(&keys[1]);
}
