#ifndef TIMER_DRIVER_H
#define TIMER_DRIVER_H

#include "main.h"

void Timer_GameTickInit(uint32_t period_ms);
void Timer_SetPeriod(uint32_t period_ms);
void Timer_Start(void);
void Timer_Stop(void);

#endif /* TIMER_DRIVER_H */