#ifndef DOOR_SWITCH_H
#define DOOR_SWITCH_H

#include <stdint.h>

void DoorSwitch_Init(void);
uint8_t DoorSwitch_Sample(uint32_t now_ms);

#endif
