#include "lcd_driver.h"

#define LCD_BL   0x08
#define LCD_EN   0x04
#define LCD_RW   0x02
#define LCD_RS   0x01

/* Stores the auto-detected I2C address and pin map */
uint8_t detected_lcd_addr = LCD_I2C_ADDR;
uint8_t detected_pin_remap = 1; /* 1 = PB8/PB9 (D15/D14), 0 = PB6/PB7 */

static void Delay_us(uint32_t us)
{
    for (volatile uint32_t i = 0; i < (us * 8); i++) { }
}

static void Delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
        Delay_us(1000);
}

static void I2C_ConfigPins(uint8_t use_remap_pb8_pb9)
{
    /* Enable AFIO, GPIOB, and I2C1 clocks */
    RCC->APB2ENR |= (1UL << 0) | (1UL << 3);
    RCC->APB1ENR |= (1UL << 21);

    /* Reset I2C1 peripheral completely */
    RCC->APB1RSTR |=  (1UL << 21);
    Delay_us(10);
    RCC->APB1RSTR &= ~(1UL << 21);

    if (use_remap_pb8_pb9)
    {
        /* Remap I2C1 to PB8 (D15=SCL) and PB9 (D14=SDA) */
        AFIO->MAPR |= (1UL << 1);
        GPIOB->CRH &= ~(0xFFUL << 0);
        GPIOB->CRH |=  (0xFFUL << 0); /* AF Open-Drain 50MHz */
    }
    else
    {
        /* Default I2C1 on PB6 (SCL) and PB7 (SDA) */
        AFIO->MAPR &= ~(1UL << 1);
        GPIOB->CRL &= ~(0xFFUL << 24);
        GPIOB->CRL |=  (0xFFUL << 24); /* AF Open-Drain 50MHz */
    }

    /* Reset I2C1 software bit */
    I2C1->CR1 |= (1UL << 15);
    I2C1->CR1 &= ~(1UL << 15);

    /* Configure I2C1 for 100kHz at 8MHz PCLK1 */
    I2C1->CR2   = 8;
    I2C1->CCR   = 40;
    I2C1->TRISE = 9;
    I2C1->CR1  |= (1UL << 0);
}

/* Probe if a device ACKs at 'addr' (7-bit) */
static bool I2C_ProbeAddress(uint8_t addr)
{
    uint32_t timeout = 10000;
    while ((I2C1->SR2 & (1UL << 1)) && --timeout) { }
    if (!timeout) return false;

    /* Generate START */
    I2C1->CR1 |= (1UL << 8);
    timeout = 10000;
    while (!(I2C1->SR1 & (1UL << 0)) && --timeout) { }
    if (!timeout) return false;

    /* Send Address */
    I2C1->DR = (addr << 1);
    timeout = 10000;
    while (!(I2C1->SR1 & ((1UL << 1) | (1UL << 10))) && --timeout) { }

    bool acked = (I2C1->SR1 & (1UL << 1)) != 0; /* Check ADDR bit */
    (void)I2C1->SR2;                            /* Clear ADDR if set */
    I2C1->SR1 &= ~(1UL << 10);                  /* Clear AF (NACK) flag */

    /* Generate STOP */
    I2C1->CR1 |= (1UL << 9);
    Delay_us(100);
    return acked;
}

static void PCF8574_Write(uint8_t data)
{
    uint32_t timeout = 20000;
    while ((I2C1->SR2 & (1UL << 1)) && --timeout) { }

    /* START */
    I2C1->CR1 |= (1UL << 8);
    timeout = 20000;
    while (!(I2C1->SR1 & (1UL << 0)) && --timeout) { }

    /* Send 7-bit address + Write bit */
    I2C1->DR = (detected_lcd_addr << 1);
    timeout = 20000;
    while (!(I2C1->SR1 & ((1UL << 1) | (1UL << 10))) && --timeout) { }

    /* If NACK (AF bit set), abort cleanly with STOP so we never hang */
    if ((I2C1->SR1 & (1UL << 10)) || !timeout)
    {
        I2C1->SR1 &= ~(1UL << 10);
        I2C1->CR1 |= (1UL << 9);
        return;
    }

    (void)I2C1->SR2; /* Clear ADDR */

    /* Write Data Byte */
    timeout = 20000;
    while (!(I2C1->SR1 & (1UL << 7)) && --timeout) { }
    I2C1->DR = data;

    timeout = 20000;
    while (!(I2C1->SR1 & (1UL << 2)) && --timeout) { }

    /* STOP */
    I2C1->CR1 |= (1UL << 9);
    Delay_us(50);
}

static void LCD_SendNibble(uint8_t nibble, uint8_t flags)
{
    uint8_t data = (nibble << 4) | flags | LCD_BL;
    PCF8574_Write(data | LCD_EN);
    Delay_us(5);
    PCF8574_Write(data & ~LCD_EN);
    Delay_us(50);
}

static void LCD_SendByte(uint8_t byte, uint8_t flags)
{
    LCD_SendNibble(byte >> 4, flags);
    LCD_SendNibble(byte & 0x0F, flags);
}

static void LCD_Command(uint8_t cmd)
{
    LCD_SendByte(cmd, 0);
}

static void LCD_Data(uint8_t data)
{
    LCD_SendByte(data, LCD_RS);
}

void LCD_Init(void)
{
    Delay_ms(50);

    /* 1. Scan D15/D14 (PB8/PB9) first, then PB6/PB7 */
    bool found = false;
    for (int remap = 1; remap >= 0 && !found; remap--)
    {
        I2C_ConfigPins((uint8_t)remap);
        Delay_ms(5);
        for (uint8_t addr = 0x20; addr <= 0x3F; addr++)
        {
            if (I2C_ProbeAddress(addr))
            {
                detected_lcd_addr = addr;
                detected_pin_remap = (uint8_t)remap;
                found = true;
                break;
            }
        }
    }

    /* If nothing found, default back to D15/D14 (PB8/PB9) and 0x27 */
    if (!found)
    {
        detected_pin_remap = 1;
        detected_lcd_addr = 0x27;
        I2C_ConfigPins(1);
    }

    /* 4-bit HD44780 initialization sequence */
    LCD_SendNibble(0x03, 0); Delay_ms(5);
    LCD_SendNibble(0x03, 0); Delay_ms(1);
    LCD_SendNibble(0x03, 0); Delay_ms(1);
    LCD_SendNibble(0x02, 0); Delay_ms(1);

    LCD_Command(0x28); Delay_ms(1); /* 4-bit, 2 lines, 5x8 */
    LCD_Command(0x08); Delay_ms(1); /* Display off */
    LCD_Command(0x01); Delay_ms(2); /* Clear */
    LCD_Command(0x06); Delay_ms(1); /* Entry mode */
    LCD_Command(0x0C); Delay_ms(1); /* Display ON, Cursor OFF */
}

void LCD_Clear(void)
{
    LCD_Command(0x01);
    Delay_ms(2);
}

void LCD_SetCursor(uint8_t col, uint8_t row)
{
    uint8_t addr = (row == 0) ? col : (0x40 + col);
    LCD_Command(0x80 | addr);
}

void LCD_PrintChar(char c)
{
    LCD_Data((uint8_t)c);
}

void LCD_PrintString(const char *str)
{
    while (*str)
        LCD_Data((uint8_t)(*str++));
}

void LCD_CreateCustomChar(uint8_t slot, const uint8_t *pattern)
{
    LCD_Command(0x40 | ((slot & 0x07) << 3));
    for (uint8_t i = 0; i < 8; i++)
        LCD_Data(pattern[i]);
    LCD_Command(0x80);
}

void LCD_WriteCustomChar(uint8_t slot)
{
    LCD_Data(slot);
}