#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define LCD_I2C_ADDR    0x27
#define LCD_COLS        16
#define LCD_ROWS        2

void LCD_Init(void);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t col, uint8_t row);
void LCD_PrintString(const char *str);
void LCD_PrintChar(char c);
void LCD_CreateCustomChar(uint8_t slot, const uint8_t *pattern);
void LCD_WriteCustomChar(uint8_t slot);

#endif /* LCD_DRIVER_H */