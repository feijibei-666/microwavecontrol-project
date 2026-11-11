#ifndef OUTPUTS_H
#define OUTPUTS_H

#include <stdint.h>

void Outputs_Init(void);
void Outputs_Apply(uint8_t door_closed, uint8_t running);
void Outputs_EmergencyStop(void);

#endif
