#include "main.h"
#include "lcd_driver.h"
#include "snake_game.h"

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
static void UART_Print(const char *s);
static uint16_t ADC_Read(uint8_t channel);
static void LCD_Render(void);
static void Process_Joystick(void);
static void Process_Button(void);

/* Custom char patterns */
static const uint8_t CHAR_HEAD[8]  = {0x0E,0x1F,0x1B,0x1F,0x1F,0x1F,0x0E,0x00};
static const uint8_t CHAR_BODY[8]  = {0x00,0x0E,0x1F,0x1F,0x1F,0x0E,0x00,0x00};
static const uint8_t CHAR_FOOD[8]  = {0x00,0x04,0x0E,0x1F,0x1F,0x0E,0x04,0x00};

/* Game tick flag set by TIM2 ISR */
volatile bool game_tick_flag = false;

/* Delay using raw loop at 8MHz HSI */
static void Delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
        for (volatile uint32_t j = 0; j < 8000; j++) { }
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

    /* Show splash screen */
    LCD_Clear();
    LCD_SetCursor(3, 0);
    LCD_PrintString("SNAKE GAME");
    LCD_SetCursor(2, 1);
    LCD_PrintString("Press USER btn");
    Delay_ms(2000);

    /* Init game */
    Snake_Init();

    /* Start game tick timer */
    TIM2_Init();

    UART_Print("\r\n=== SNAKE GAME STARTED ===\r\n");
    UART_Print("USER button = pause/resume\r\n\r\n");

    while (1)
    {
        /* Process joystick input */
        Process_Joystick();

        /* Process USER button for pause */
        Process_Button();

        /* Game tick */
        if (game_tick_flag)
        {
            game_tick_flag = false;

            GameState_t state = Snake_GetState();

            if (state == STATE_IDLE)
            {
                /* Wait for button press to start */
            }
            else if (state == STATE_PLAYING)
            {
                Snake_Tick();
                LCD_Render();

                /* UART telemetry */
                char buf[64];
                const char *dir_str[] = {"UP","DOWN","LEFT","RIGHT"};
                snprintf(buf, sizeof(buf),
                    "Score:%u Len:%u Dir:%s\r\n",
                    Snake_GetScore(),
                    Snake_GetLength(),
                    dir_str[Snake_GetDir()]);
                UART_Print(buf);
            }
            else if (state == STATE_GAME_OVER)
            {
                LCD_Clear();
                LCD_SetCursor(3, 0);
                LCD_PrintString("GAME  OVER");
                char buf[16];
                snprintf(buf, sizeof(buf), "Score: %u", Snake_GetScore());
                LCD_SetCursor(0, 1);
                LCD_PrintString(buf);
                UART_Print("GAME OVER\r\n");

                /* Wait for button to restart */
                Delay_ms(2000);
                while (GPIOC->IDR & (1UL << 13)) { } /* Wait press */
                Snake_Reset();
            }
            else if (state == STATE_PAUSED)
            {
                LCD_SetCursor(5, 1);
                LCD_PrintString("PAUSED");
            }
        }
    }
}

/* -------------------------------------------------------
 * LCD Render
 * ------------------------------------------------------- */
extern Point_t *Snake_GetBody(void);
extern Point_t  Snake_GetFood(void);

static void LCD_Render(void)
{
    /* Build 16x2 display buffer */
    uint8_t grid[2][16];
    memset(grid, ' ', sizeof(grid));

    Point_t food = Snake_GetFood();
    grid[food.row][food.col] = 2; /* food char */

    Point_t *body = Snake_GetBody();
    uint8_t  len  = Snake_GetLength();

    for (uint8_t i = len - 1; i > 0; i--)
        grid[body[i].row][body[i].col] = 1; /* body char */

    grid[body[0].row][body[0].col] = 0; /* head char */

    /* Write to LCD */
    LCD_SetCursor(0, 0);
    for (uint8_t c = 0; c < 16; c++) LCD_WriteCustomChar(grid[0][c]);
    LCD_SetCursor(0, 1);
    for (uint8_t c = 0; c < 16; c++) LCD_WriteCustomChar(grid[1][c]);
}

/* -------------------------------------------------------
 * Joystick processing
 * -------------------------------------------------------
 * ADC mid = ~2048, deadzone = 800
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
        /* Falling edge — button pressed */
        GameState_t s = Snake_GetState();
        if (s == STATE_IDLE)
        {
            Snake_Reset();
            LCD_Clear();
        }
        else if (s == STATE_PLAYING) Snake_Pause();
        else if (s == STATE_PAUSED)  Snake_Resume();
    }
    btn_last = btn_now;
    Delay_ms(10); /* debounce */
}

/* -------------------------------------------------------
 * GPIO Init
 * ------------------------------------------------------- */
static void GPIO_Init(void)
{
    /* Enable GPIOA, GPIOC */
    RCC->APB2ENR |= (1UL << 2) | (1UL << 4);

    /* PA5 = LD2 output */
    GPIOA->CRL &= ~(0xFUL << 20);
    GPIOA->CRL |=  (0x2UL << 20);

    /* PC13 = USER button input pull-up */
    GPIOC->CRH &= ~(0xFUL << 20);
    GPIOC->CRH |=  (0x8UL << 20); /* Input with pull-up/pull-down */
    GPIOC->ODR |=  (1UL << 13);   /* Pull-up */
}

/* -------------------------------------------------------
 * ADC Init — PA0 and PA1
 * ------------------------------------------------------- */
static void ADC_Init(void)
{
    /* Enable ADC1 and GPIOA clocks */
    RCC->APB2ENR |= (1UL << 9) | (1UL << 2);

    /* PA0, PA1 = analog input */
    GPIOA->CRL &= ~(0xFFUL << 0);  /* Clear CNF and MODE for PA0 and PA1 */

    /* ADC prescaler /6 = 12MHz max */
    RCC->CFGR &= ~(3UL << 14);
    RCC->CFGR |=  (2UL << 14);

    /* ADC1 config */
    ADC1->CR2  = (1UL << 0);  /* ADON */
    ADC1->SMPR2 = (7UL << 0) | (7UL << 3); /* 239.5 cycles for CH0 and CH1 */
    ADC1->SQR3  = 0; /* Start with channel 0 */

    /* Calibrate */
    ADC1->CR2 |= (1UL << 2); /* CAL */
    while (ADC1->CR2 & (1UL << 2)) { }
}

static uint16_t ADC_Read(uint8_t channel)
{
    ADC1->SQR3 = channel;
    ADC1->CR2 |= (1UL << 0);   /* ADON re-trigger */
    ADC1->CR2 |= (1UL << 22);  /* SWSTART */
    while (!(ADC1->SR & (1UL << 1))) { } /* Wait EOC */
    return (uint16_t)ADC1->DR;
}

/* -------------------------------------------------------
 * UART2 Init — PA2=TX at 9600 baud, 8MHz HSI
 * ------------------------------------------------------- */
static void UART_Init(void)
{
    RCC->APB1ENR |= (1UL << 17); /* USART2 */
    RCC->APB2ENR |= (1UL << 2);  /* GPIOA  */

    /* PA2 = UART2 TX, AF push-pull */
    GPIOA->CRL &= ~(0xFUL << 8);
    GPIOA->CRL |=  (0xBUL << 8); /* AF push-pull 50MHz */

    USART2->BRR = 833;  /* 8MHz / 9600 = 833 */
    USART2->CR1 = (1UL << 3) | (1UL << 13); /* TE + UE */
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
 * TIM2 — game tick at ~500ms (adjustable)
 * At 8MHz, PSC=7999, ARR=499 → 1ms * 500 = 500ms tick
 * ------------------------------------------------------- */
static void TIM2_Init(void)
{
    RCC->APB1ENR |= (1UL << 0); /* TIM2 */

    TIM2->PSC  = 7999;  /* 8MHz / 8000 = 1kHz */
    TIM2->ARR  = 499;   /* 1kHz / 500 = 2Hz = 500ms tick */
    TIM2->DIER = (1UL << 0); /* Update interrupt enable */
    TIM2->CR1  = (1UL << 0); /* Enable TIM2 */

    /* Enable TIM2 interrupt in NVIC */
    NVIC_SetPriority(TIM2_IRQn, 2);
    NVIC_EnableIRQ(TIM2_IRQn);
}

/* -------------------------------------------------------
 * TIM2 ISR
 * ------------------------------------------------------- */
void TIM2_IRQHandler(void)
{
    if (TIM2->SR & (1UL << 0))
    {
        TIM2->SR &= ~(1UL << 0); /* Clear flag */
        game_tick_flag = true;
        GPIOA->ODR ^= (1UL << 5); /* Heartbeat on LD2 */
    }
}