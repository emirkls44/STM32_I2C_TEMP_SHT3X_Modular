/* Minimal host stand-in for the STM32 HAL, used only by the tests. */
#ifndef STM32F0XX_HAL_H
#define STM32F0XX_HAL_H
#include "stm32f0xx.h"
uint32_t HAL_GetTick(void);
#endif
