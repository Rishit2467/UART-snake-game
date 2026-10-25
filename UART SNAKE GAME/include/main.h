/* main.h — Top-level includes and clock configuration declarations */
#ifndef MAIN_H
#define MAIN_H

/* STM32Cube HAL — must be first */
#include "stm32f1xx_hal.h"

/* Standard library */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/* ---------------------------------------------------------------
 * Clock configuration target
 *   SYSCLK  = 72 MHz  (PLL × HSE 8 MHz × 9)
 *   AHB     = 72 MHz  (prescaler /1)
 *   APB1    = 36 MHz  (prescaler /2)  ← I2C1, USART2, TIM2/3 live here
 *   APB2    = 72 MHz  (prescaler /1)  ← ADC1 lives here (ADC clk = 72/6 = 12 MHz)
 * --------------------------------------------------------------- */

/* ---------------------------------------------------------------
 * Error handler — called on HAL assert failures or init errors
 * --------------------------------------------------------------- */
void Error_Handler(void);

/* ---------------------------------------------------------------
 * System clock init — implemented in main.c
 * --------------------------------------------------------------- */
void SystemClock_Config(void);

#endif /* MAIN_H */