#include "main.h"
#include "lcd_driver.h"
#include "snake_game.h"
#include <stdlib.h>

/* Suppress __libc_init_array crash */
void __libc_init_array(void) { }
void SystemClock_Config(void) { }
void Error_Handler(void) { __disable_irq(); while(1){} }

/* -------------------------------------------------------
 * Forward declarations
 * ------------------------------------------------------- */
static void GPIO_Init(void);
static void ADC_Init(void);
static void UART_Init(void);
static void TIM2_Init(void);
static void TIM2_SetSpeed(uint8_t speed);
static void UART_Print(const char *s);
static uint16_t ADC_Read(uint8_t channel);
static void LCD_Render(void);
static void Process_Joystick(void);
static void Process_Button(void);
static void Process_UART_Command(char *cmd);

/* Custom char patterns */
static const uint8_t CHAR_HEAD[8]  = {0x0E,0x1F,0x1B,0x1F,0x1F,0x1F,0x0E,0x00};
static const uint8_t CHAR_BODY[8]  = {0x00,0x0E,0x1F,0x1F,0x1F,0x0E,0x00,0x00};
static const uint8_t CHAR_FOOD[8]  = {0x00,0x04,0x0E,0x1F,0x1F,0x0E,0x04,0x00};

/* Game tick flag set by TIM2 ISR */
volatile bool game_tick_flag = false;
static uint8_t current_speed = 3;
static GameState_t last_rendered_state = (GameState_t)0xFF;

/* UART RX Interrupt Buffer */
#define UART_BUF_SIZE 64
static volatile char uart_rx_buf[UART_BUF_SIZE];
static volatile uint8_t uart_rx_idx = 0;
static volatile bool uart_cmd_ready = false;

/* Delay using raw loop at 8MHz HSI */
static void Delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
        for (volatile uint32_t j = 0; j < 800; j++) { }
}

/* -------------------------------------------------------
 * main
 * ------------------------------------------------------- */
int main(void)
{
    GPIO_Init();
    ADC_Init();
    UART_Init();

    /* LCD init and custom chars */
    LCD_Init();
    LCD_CreateCustomChar(0, CHAR_HEAD);
    LCD_CreateCustomChar(1, CHAR_BODY);
    LCD_CreateCustomChar(2, CHAR_FOOD);

    /* Print I2C Diagnostic Info to Terminal */
    extern uint8_t detected_lcd_addr;
    extern uint8_t detected_pin_remap;
    char diag[80];
    snprintf(diag, sizeof(diag),
        "\r\n[I2C] Using Addr: 0x%02X on Pins: %s\r\n",
        detected_lcd_addr,
        detected_pin_remap ? "PB8/PB9 (D15/D14)" : "PB6/PB7 (D10)");
    UART_Print(diag);

    /* Show splash screen */
    LCD_Clear();
    LCD_SetCursor(3, 0);
    LCD_PrintString("SNAKE GAME");
    LCD_SetCursor(1, 1);
    LCD_PrintString("Press USER btn");

    /* Init game in IDLE state */
    Snake_Init();
    last_rendered_state = STATE_IDLE;

    /* Start game tick timer (default speed 3) */
    TIM2_Init();
    TIM2_SetSpeed(3);

    UART_Print("=== SNAKE GAME READY (9600 BAUD) ===\r\n");
    UART_Print("Commands: PAUSE, RESUME, RESET, SPEED <1-5>, STATUS\r\n");
    UART_Print("USER button (PC13) = Start / Pause / Resume\r\n\r\n");

    while (1)
    {
        /* 1. Process real-time UART terminal commands */
        if (uart_cmd_ready)
        {
            char cmd_copy[UART_BUF_SIZE];
            __disable_irq();
            strncpy(cmd_copy, (const char *)uart_rx_buf, UART_BUF_SIZE);
            uart_cmd_ready = false;
            __enable_irq();

            Process_UART_Command(cmd_copy);
        }

        /* 2. Process joystick analog axes */
        Process_Joystick();

        /* 3. Process USER button (PC13) */
        Process_Button();

        /* 4. Handle TIM2 hardware timer game tick */
        if (game_tick_flag)
        {
            game_tick_flag = false;

            GameState_t state = Snake_GetState();

            if (state == STATE_PLAYING)
            {
                Snake_Tick();
                state = Snake_GetState(); /* Re-check if tick caused GAME_OVER */

                if (state == STATE_PLAYING)
                {
                    LCD_Render();
                    last_rendered_state = STATE_PLAYING;

                    /* Live UART telemetry every game tick */
                    char buf[64];
                    const char *dir_str[] = {"UP", "DOWN", "LEFT", "RIGHT"};
                    snprintf(buf, sizeof(buf),
                        "Score:%u Len:%u Dir:%s Spd:%u\r\n",
                        Snake_GetScore(),
                        Snake_GetLength(),
                        dir_str[Snake_GetDir()],
                        current_speed);
                    UART_Print(buf);
                }
            }

            /* Render state transition screens once to avoid LCD flicker */
            if (state != last_rendered_state)
            {
                last_rendered_state = state;

                if (state == STATE_IDLE)
                {
                    LCD_Clear();
                    LCD_SetCursor(3, 0);
                    LCD_PrintString("SNAKE GAME");
                    LCD_SetCursor(1, 1);
                    LCD_PrintString("Press USER btn");
                }
                else if (state == STATE_PAUSED)
                {
                    LCD_SetCursor(5, 1);
                    LCD_PrintString("PAUSED");
                }
                else if (state == STATE_GAME_OVER)
                {
                    LCD_Clear();
                    LCD_SetCursor(3, 0);
                    LCD_PrintString("GAME  OVER");
                    char buf[24]; 
                    snprintf(buf, sizeof(buf), "Score:%u Btn=Rst", Snake_GetScore());
                    LCD_SetCursor(0, 1);
                    LCD_PrintString(buf);
                    UART_Print("GAME OVER! Press USER btn or send RESET\r\n");
                }
            }
        }
    }
}

/* -------------------------------------------------------
 * LCD Render
 * ------------------------------------------------------- */
static void LCD_Render(void)
{
    uint8_t grid[2][16];
    memset(grid, ' ', sizeof(grid));

    Point_t food = Snake_GetFood();
    grid[food.row][food.col] = 2; /* food custom char */

    Point_t *body = Snake_GetBody();
    uint8_t  len  = Snake_GetLength();

    for (uint8_t i = len - 1; i > 0; i--)
        grid[body[i].row][body[i].col] = 1; /* body custom char */

    grid[body[0].row][body[0].col] = 0; /* head custom char */

    LCD_SetCursor(0, 0);
    for (uint8_t c = 0; c < 16; c++) LCD_WriteCustomChar(grid[0][c]);
    LCD_SetCursor(0, 1);
    for (uint8_t c = 0; c < 16; c++) LCD_WriteCustomChar(grid[1][c]);
}

/* -------------------------------------------------------
 * Joystick processing (PA0=VRX, PA1=VRY, powered at 3.3V)
 * ------------------------------------------------------- */
static void Process_Joystick(void)
{
    if (Snake_GetState() != STATE_PLAYING) return;

    uint16_t x = ADC_Read(0); /* PA0 */
    uint16_t y = ADC_Read(1); /* PA1 */

    if (x > 3000)      Snake_SetDirection(DIR_RIGHT);
    else if (x < 1000) Snake_SetDirection(DIR_LEFT);
    else if (y > 3000) Snake_SetDirection(DIR_DOWN);
    else if (y < 1000) Snake_SetDirection(DIR_UP);
}

/* -------------------------------------------------------
 * Button processing — USER button PC13, active LOW
 * ------------------------------------------------------- */
static bool btn_last = true;
static void Process_Button(void)
{
    bool btn_now = (GPIOC->IDR & (1UL << 13)) ? true : false;

    if (btn_last && !btn_now)
    {
        GameState_t s = Snake_GetState();
        if (s == STATE_IDLE || s == STATE_GAME_OVER)
        {
            LCD_Clear();
            Snake_Reset();
            last_rendered_state = STATE_PLAYING;
            UART_Print("[BTN] Game Started\r\n");
        }
        else if (s == STATE_PLAYING)
        {
            Snake_Pause();
            UART_Print("[BTN] Game Paused\r\n");
        }
        else if (s == STATE_PAUSED)
        {
            Snake_Resume();
            UART_Print("[BTN] Game Resumed\r\n");
        }
    }
    btn_last = btn_now;
    Delay_ms(10);
}

/* -------------------------------------------------------
 * UART Command Parser
 * Supports: PAUSE, RESUME, RESET, SPEED <1-5>, STATUS
 * ------------------------------------------------------- */
static void Process_UART_Command(char *cmd)
{
    /* Trim trailing whitespace / CR / LF */
    int len = strlen(cmd);
    while (len > 0 && (cmd[len - 1] == '\r' || cmd[len - 1] == '\n' || cmd[len - 1] == ' '))
    {
        cmd[--len] = '\0';
    }
    if (len == 0) return;

    if (strcmp(cmd, "PAUSE") == 0)
    {
        Snake_Pause();
        UART_Print("[OK] PAUSED\r\n");
    }
    else if (strcmp(cmd, "RESUME") == 0)
    {
        if (Snake_GetState() == STATE_IDLE)
            Snake_Reset();
        else
            Snake_Resume();
        UART_Print("[OK] RESUMED\r\n");
    }
    else if (strcmp(cmd, "RESET") == 0)
    {
        LCD_Clear();
        Snake_Reset();
        last_rendered_state = STATE_PLAYING;
        UART_Print("[OK] RESET\r\n");
    }
    else if (strncmp(cmd, "SPEED ", 6) == 0)
    {
        int spd = atoi(&cmd[6]);
        if (spd >= 1 && spd <= 5)
        {
            TIM2_SetSpeed((uint8_t)spd);
            char buf[32];
            snprintf(buf, sizeof(buf), "[OK] SPEED %d\r\n", spd);
            UART_Print(buf);
        }
        else
        {
            UART_Print("[ERR] Use SPEED 1 to 5\r\n");
        }
    }
    else if (strcmp(cmd, "STATUS") == 0)
    {
        const char *state_str[] = {"IDLE", "PLAYING", "PAUSED", "GAME_OVER"};
        const char *dir_str[]   = {"UP", "DOWN", "LEFT", "RIGHT"};
        char buf[96];
        snprintf(buf, sizeof(buf),
            "[STATUS] State:%s Score:%u Len:%u Dir:%s Speed:%u\r\n",
            state_str[Snake_GetState()],
            Snake_GetScore(),
            Snake_GetLength(),
            dir_str[Snake_GetDir()],
            current_speed);
        UART_Print(buf);
    }
    else
    {
        UART_Print("[ERR] Unknown cmd. Valid: PAUSE, RESUME, RESET, SPEED <1-5>, STATUS\r\n");
    }
}

/* -------------------------------------------------------
 * GPIO Init
 * ------------------------------------------------------- */
static void GPIO_Init(void)
{
    RCC->APB2ENR |= (1UL << 2) | (1UL << 4); /* GPIOA, GPIOC */

    /* PA5 = LD2 output */
    GPIOA->CRL &= ~(0xFUL << 20);
    GPIOA->CRL |=  (0x2UL << 20);

    /* PC13 = USER button input pull-up */
    GPIOC->CRH &= ~(0xFUL << 20);
    GPIOC->CRH |=  (0x8UL << 20);
    GPIOC->ODR |=  (1UL << 13);
}

/* -------------------------------------------------------
 * ADC Init — PA0 and PA1
 * ------------------------------------------------------- */
static void ADC_Init(void)
{
    RCC->APB2ENR |= (1UL << 9) | (1UL << 2);

    /* PA0, PA1 = analog input */
    GPIOA->CRL &= ~(0xFFUL << 0);

    /* ADC prescaler /6 */
    RCC->CFGR &= ~(3UL << 14);
    RCC->CFGR |=  (2UL << 14);

    ADC1->CR2   = (1UL << 0); /* ADON */
    ADC1->SMPR2 = (7UL << 0) | (7UL << 3);
    ADC1->SQR3  = 0;

    /* Calibrate */
    ADC1->CR2 |= (1UL << 2);
    while (ADC1->CR2 & (1UL << 2)) { }
}

static uint16_t ADC_Read(uint8_t channel)
{
    ADC1->SQR3 = channel;
    ADC1->CR2 |= (1UL << 0);
    ADC1->CR2 |= (1UL << 22);
    while (!(ADC1->SR & (1UL << 1))) { }
    return (uint16_t)ADC1->DR;
}

/* -------------------------------------------------------
 * UART2 Init — PA2=TX, PA3=RX at 9600 baud (8MHz HSI)
 * Interrupt-driven RX enabled
 * ------------------------------------------------------- */
static void UART_Init(void)
{
    RCC->APB1ENR |= (1UL << 17); /* USART2 */
    RCC->APB2ENR |= (1UL << 2);  /* GPIOA  */

    /* PA2 = TX (AF push-pull 50MHz), PA3 = RX (Input floating) */
    GPIOA->CRL &= ~(0xFFUL << 8);
    GPIOA->CRL |=  (0x4BUL << 8); /* PA2=0xB, PA3=0x4 */

    USART2->BRR = 833; /* 8MHz / 9600 = 833 */
    /* Enable RXNEIE (bit 5), RE (bit 2), TE (bit 3), UE (bit 13) */
    USART2->CR1 = (1UL << 13) | (1UL << 5) | (1UL << 3) | (1UL << 2);

    NVIC_SetPriority(USART2_IRQn, 1);
    NVIC_EnableIRQ(USART2_IRQn);
}

static void UART_Print(const char *s)
{
    while (*s)
    {
        while (!(USART2->SR & (1UL << 7))) { }
        USART2->DR = (uint8_t)(*s++);
    }
}

/* -------------------------------------------------------
 * USART2 Interrupt Handler (Real-time PC Terminal RX)
 * ------------------------------------------------------- */
void USART2_IRQHandler(void)
{
    if (USART2->SR & (1UL << 5)) /* RXNE */
    {
        char c = (char)(USART2->DR & 0xFF);
        if (c == '\n' || c == '\r')
        {
            if (uart_rx_idx > 0)
            {
                uart_rx_buf[uart_rx_idx] = '\0';
                uart_rx_idx = 0;
                uart_cmd_ready = true;
            }
        }
        else if (uart_rx_idx < (UART_BUF_SIZE - 1))
        {
            /* Auto-uppercase for convenience */
            if (c >= 'a' && c <= 'z') c -= 32;
            uart_rx_buf[uart_rx_idx++] = c;
        }
    }
}

/* -------------------------------------------------------
 * TIM2 — Hardware Timer Interrupt for Game Tick
 * At 8MHz, PSC=7999 -> 1kHz (1ms per tick)
 * ------------------------------------------------------- */
static void TIM2_Init(void)
{
    RCC->APB1ENR |= (1UL << 0); /* TIM2 */

    TIM2->PSC  = 7999; /* 8MHz / 8000 = 1kHz */
    TIM2->ARR  = 499;  /* Default 500ms */
    TIM2->DIER = (1UL << 0);
    TIM2->CR1  = (1UL << 0);

    NVIC_SetPriority(TIM2_IRQn, 2);
    NVIC_EnableIRQ(TIM2_IRQn);
}

static void TIM2_SetSpeed(uint8_t speed)
{
    /* Speed 1..5 -> 700ms, 550ms, 400ms, 280ms, 160ms */
    static const uint16_t arr_table[5] = {699, 549, 399, 279, 159};
    if (speed < 1) speed = 1;
    if (speed > 5) speed = 5;
    current_speed = speed;

    TIM2->ARR = arr_table[speed - 1];
    TIM2->CNT = 0;
}

void TIM2_IRQHandler(void)
{
    if (TIM2->SR & (1UL << 0))
    {
        TIM2->SR &= ~(1UL << 0);
        game_tick_flag = true;
        GPIOA->ODR ^= (1UL << 5); /* Heartbeat on LD2 */
    }
}