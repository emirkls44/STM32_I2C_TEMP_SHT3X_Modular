/*
 * SHT3x temperature/humidity demo for the STM32F0DISCOVERY (STM32F051R8).
 *
 * The sensor is read once per second by a non-blocking driver. Results are
 * kept in the g_* variables below; watch them in the debugger (STM32CubeIDE:
 * Window -> Show View -> Live Expressions). The green LED (PC9) toggles on
 * every valid measurement.
 *
 * Note: this main.c is written by hand. The I2C peripheral is driven at
 * register level (i2c.c) and is not configured in the .ioc file, so do not
 * let STM32CubeMX regenerate this file.
 */
#include "i2c.h"
#include "sht3x.h"
#include "stm32f0xx_hal.h"

/* Debugger-visible results */
volatile float g_temperature_c = 0.0f;
volatile float g_humidity_rh = 0.0f;
volatile SHT3x_Stats_t g_sht3x_stats;

static void led_init(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC->MODER = (GPIOC->MODER & ~GPIO_MODER_MODER9) | GPIO_MODER_MODER9_0; /* PC9 output */
}

static void led_toggle(void)
{
    GPIOC->ODR ^= GPIO_ODR_9;
}

int main(void)
{
    HAL_Init(); /* SysTick 1 ms for HAL_GetTick(); core runs from the 8 MHz HSI */
    led_init();
    I2C1_Init();

    for (;;)
    {
        SHT3x_Update();

        SHT3x_Measurement_t measurement;

        if (SHT3x_GetMeasurement(&measurement))
        {
            g_temperature_c = measurement.temperature_c;
            g_humidity_rh = measurement.humidity_rh;
            led_toggle();
        }

        g_sht3x_stats = SHT3x_GetStats();
    }
}
