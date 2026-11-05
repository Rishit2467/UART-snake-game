#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include "main.h"

void ADC_JoystickInit(void);
uint16_t ADC_ReadX(void);   /* PA0 — raw 0..4095 */
uint16_t ADC_ReadY(void);   /* PA1 — raw 0..4095 */

#endif /* ADC_DRIVER_H */