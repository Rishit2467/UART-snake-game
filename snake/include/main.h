#ifndef MAIN_H
#define MAIN_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

void Error_Handler(void);
void SystemClock_Config(void);
void __libc_init_array(void);

#endif