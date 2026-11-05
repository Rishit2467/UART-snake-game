#include "lcd_driver.h"

/* PCF8574 bit mapping:
 * P7=D7 P6=D6 P5=D5 P4=D4 P3=BL P2=EN P1=RW P0=RS */
#define LCD_BL   0x08
#define LCD_EN   0x04
#define LCD_RW   0x02
#define LCD_RS   0x01

/* I2C1 register base */
#define I2C1_BASE_ADDR  0x40005400UL

static void I2C_Init(void);
static void I2C_Start(void);
static void I2C_Stop(void);
static void I2C_WriteByte(uint8_t data);
static void I2C_WaitBusy(void);
static void LCD_SendNibble(uint8_t nibble, uint8_t flags);
static void LCD_SendByte(uint8_t byte, uint8_t flags);
static void LCD_Command(uint8_t cmd);
static void LCD_Data(uint8_t data);
static void Delay_us(uint32_t us);
static void Delay_ms(uint32_t ms);

static void Delay_us(uint32_t us)
{
    /* At 8MHz HSI, roughly 8 cycles per us */
    for (volatile uint32_t i = 0; i < (us * 8); i++) { }
}

static void Delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
        Delay_us(1000);
}

static void I2C_Init(void)
{
    /* Enable GPIOB and I2C1 clocks */
    RCC->APB2ENR |= (1UL << 3);   /* GPIOB */
    RCC->APB1ENR |= (1UL << 21);  /* I2C1  */

    /* PB6=SCL, PB7=SDA: AF open-drain, 50MHz */
    GPIOB->CRL &= ~(0xFFUL << 24);
    GPIOB->CRL |=  (0xFFUL << 24); /* AF open-drain 50MHz for PB6 and PB7 */

    /* Reset I2C1 */
    I2C1->CR1 |= (1UL << 15);
    I2C1->CR1 &= ~(1UL << 15);

    /* Configure I2C1 for 100kHz at 8MHz PCLK1 */
    I2C1->CR2   = 8;           /* PCLK1 = 8MHz */
    I2C1->CCR   = 40;          /* 100kHz: 8MHz / (2*100kHz) = 40 */
    I2C1->TRISE = 9;           /* (8MHz * 1000ns) + 1 = 9 */
    I2C1->CR1  |= (1UL << 0);  /* Enable I2C1 */
}

static void I2C_WaitBusy(void)
{
    uint32_t timeout = 100000;
    while ((I2C1->SR2 & (1UL << 1)) && --timeout) { }
}

static void I2C_Start(void)
{
    I2C_WaitBusy();
    I2C1->CR1 |= (1UL << 8);   /* START */
    uint32_t timeout = 100000;
    while (!(I2C1->SR1 & (1UL << 0)) && --timeout) { } /* Wait SB */
}

static void I2C_Stop(void)
{
    I2C1->CR1 |= (1UL << 9);   /* STOP */
    uint32_t timeout = 100000;
    while ((I2C1->SR1 & (1UL << 9)) && --timeout) { }
}

static void I2C_WriteByte(uint8_t data)
{
    uint32_t timeout = 100000;
    while (!(I2C1->SR1 & (1UL << 7)) && --timeout) { } /* Wait TXE */
    I2C1->DR = data;
    timeout = 100000;
    while (!(I2C1->SR1 & (1UL << 2)) && --timeout) { } /* Wait BTF */
}

static void PCF8574_Write(uint8_t data)
{
    I2C_Start();
    /* Send address with write bit */
    I2C1->DR = (LCD_I2C_ADDR << 1);
    uint32_t timeout = 100000;
    while (!(I2C1->SR1 & (1UL << 1)) && --timeout) { } /* Wait ADDR */
    (void)I2C1->SR2; /* Clear ADDR */
    I2C_WriteByte(data);
    I2C_Stop();
    Delay_us(50);
}

static void LCD_SendNibble(uint8_t nibble, uint8_t flags)
{
    uint8_t data = (nibble << 4) | flags | LCD_BL;
    PCF8574_Write(data | LCD_EN);
    Delay_us(1);
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
    I2C_Init();
    Delay_ms(50);

    /* 4-bit init sequence */
    LCD_SendNibble(0x03, 0); Delay_ms(5);
    LCD_SendNibble(0x03, 0); Delay_ms(1);
    LCD_SendNibble(0x03, 0); Delay_ms(1);
    LCD_SendNibble(0x02, 0); Delay_ms(1);

    /* Function set: 4-bit, 2 lines, 5x8 */
    LCD_Command(0x28); Delay_ms(1);
    /* Display off */
    LCD_Command(0x08); Delay_ms(1);
    /* Clear */
    LCD_Command(0x01); Delay_ms(2);
    /* Entry mode: increment, no shift */
    LCD_Command(0x06); Delay_ms(1);
    /* Display on, cursor off, blink off */
    LCD_Command(0x0C); Delay_ms(1);
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
    LCD_Command(0x40 | (slot << 3));
    for (uint8_t i = 0; i < 8; i++)
        LCD_Data(pattern[i]);
    LCD_Command(0x80); /* Return to DDRAM */
}

void LCD_WriteCustomChar(uint8_t slot)
{
    LCD_Data(slot);
}