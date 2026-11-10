#ifndef MICROWAVE_UI_H
#define MICROWAVE_UI_H

#include "app.h"

void UI_Init(void);
void UI_SetSnapshot(const AppSnapshot *snapshot, uint32_t now);
void UI_RenderStep(uint32_t now);

/* Diagnostics: updated only by the display task. */
extern volatile uint32_t ui_rendered_bands;
extern volatile uint32_t ui_transition_count;

#endif
