#include "stm32f1xx_hal.h"

/* Override __libc_init_array before it can fault.
 * The startup file calls this before main().
 * Our version does nothing — we have no C++ static constructors. */
void __libc_init_array(void) { }

static void GPIO_Init(void);
static void DiagUART_Init(void);
static void DiagUART_Print(const char *s);

int main(void)
{
    SystemCoreClock = 8000000U;
    SysTick_Config(8000U);

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    GPIO_Init();
    DiagUART_Init();

    DiagUART_Print("\r\nHello from STM32!\r\n");

    while (1)
    {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(500);
    }
}

static void GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    g.Pin   = GPIO_PIN_5;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
}

static void DiagUART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin   = GPIO_PIN_2;
    g.Mode  = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
    USART2->BRR = 833U;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void DiagUART_Print(const char *s)
{
    while (*s)
    {
        while (!(USART2->SR & USART_SR_TXE)) { }
        USART2->DR = (uint8_t)(*s++);
    }
}

void SystemClock_Config(void) { }
void Error_Handler(void) { __disable_irq(); while(1){} }