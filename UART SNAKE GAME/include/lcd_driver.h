#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include "main.h"

void LCD_Init(void);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t col, uint8_t row);
void LCD_PrintString(const char *str);
void LCD_PrintChar(char c);
void LCD_CreateCustomChar(uint8_t slot, const uint8_t *pattern);

#endif /* LCD_DRIVER_H */