#ifndef INT_LCC_KNOB_H
#define INT_LCC_KNOB_H

#include "tim.h"

typedef void (*KnobCallback)(void);

void Knob_Init();
void Knob_Loop();

void Knob_SetForwardCallback(KnobCallback callback);
void Knob_SetBackwardCallback(KnobCallback callback);
void Knob_SetPressedCallback(KnobCallback callback);

#endif