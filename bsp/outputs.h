#ifndef OUTPUTS_H
#define OUTPUTS_H

#include <stdint.h>

void Outputs_Init(void);
/* 返回本次下发的 PA2 吸合命令。 */
uint8_t Outputs_Apply(uint8_t door_closed, uint8_t running, uint8_t relay_requested);
void Outputs_EmergencyStop(void);

#endif
