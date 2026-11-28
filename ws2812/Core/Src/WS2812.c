#include "WS2812.h"

#define Code0    30
#define Code1    60
#define CodeRest 0

void ws2812_Update()
{
     static uint16_t date[] = {
                    Code0,Code0,Code0,Code0,Code0,Code0,Code0,Code0,
                    Code1,Code1,Code1,Code1,Code1,Code1,Code1,Code1,
                    Code0,Code0,Code0,Code0,Code0,Code0,Code0,Code0,

                    Code0,Code0,Code0,Code0,Code0,Code0,Code0,Code0,
                    Code1,Code1,Code1,Code1,Code1,Code1,Code1,Code1,
                    Code0,Code0,Code0,Code0,Code0,Code0,Code0,Code0,

                    Code0,Code0,Code0,Code0,Code0,Code0,Code0,Code0,
                    Code1,Code1,Code1,Code1,Code1,Code1,Code1,Code1,
                    Code0,Code0,Code0,Code0,Code0,Code0,Code0,Code0,

                    Code0,Code0,Code0,Code0,Code0,Code0,Code0,Code0,
                    Code1,Code1,Code1,Code1,Code1,Code1,Code1,Code1,
                    Code0,Code0,Code0,Code0,Code0,Code0,Code0,Code0,
                    CodeRest
                    };

    HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_1, (uint32_t*)date, sizeof(date)/sizeof(uint16_t ));
}