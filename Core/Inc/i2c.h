#ifndef I2C_H
#define I2C_H

/*
 * Register-level, interrupt-driven I2C1 master driver for the STM32F0.
 *
 * Pins: PB6 = SCL, PB7 = SDA (AF1, open-drain). External pull-ups are
 * required (4.7 kOhm to 3.3 V is typical).
 * Speed: 100 kHz Standard-mode. TIMINGR assumes I2C1 is clocked from the
 * 8 MHz HSI, which is the reset default (RCC_CFGR3.I2C1SW = 0) and does not
 * depend on the system clock.
 *
 * One transfer at a time. Start it with I2C1_WriteAsync()/I2C1_ReadAsync()
 * and poll I2C1_GetStatus() until it is I2C_COMPLETE or I2C_ERROR.
 */

#include "stm32f0xx.h"

#include <stdbool.h>
#include <stdint.h>

#define I2C1_MAX_WRITE_LENGTH 4U
#define I2C1_MAX_READ_LENGTH  8U

typedef enum
{
    I2C_IDLE,
    I2C_BUSY,
    I2C_ERROR, /* NACK, bus error or arbitration lost */
    I2C_COMPLETE
} I2C_Status_t;

void I2C1_Init(void);

/* Return false if a transfer is already running or the length is invalid.
 * The write data is copied, so `data` does not need to stay valid. */
bool I2C1_WriteAsync(uint8_t address_7bit, const uint8_t *data, uint8_t length);
bool I2C1_ReadAsync(uint8_t address_7bit, uint8_t length);

I2C_Status_t I2C1_GetStatus(void);

/* Copies the bytes of the last completed read. Returns the number copied. */
uint8_t I2C1_CopyReadData(uint8_t *destination, uint8_t length);

/* Aborts a transfer that never completes (e.g. SDA held low) by resetting
 * the peripheral. The status becomes I2C_IDLE. */
void I2C1_Abort(void);

#endif /* I2C_H */
