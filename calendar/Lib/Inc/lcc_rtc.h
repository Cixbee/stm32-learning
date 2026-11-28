#ifndef INT_LCC_RTC_H_
#define INT_LCC_RTC_H_

#include "stm32f1xx_hal.h"
#include "rtc.h"
#include "time.h"



HAL_StatusTypeDef LCC_RTC_SetTime(struct tm * time );


struct tm * LCC_RTC_GetTime();

void LCC_RTC_Init();

#endif 