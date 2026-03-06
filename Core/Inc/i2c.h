/*
 * i2c.h
 *
 *  Created on: 6 Mar 2026
 *      Author: Ebrar
 */

#ifndef INC_I2C_H_
#define INC_I2C_H_

#include "stm32f0xx.h"

typedef enum {
	I2C_IDLE,
	I2C_BUSY,
	I2C_ERROR,
	I2C_COMPLETE
} I2C_Status_t;

void I2C1_Init(void);

I2C_Status_t I2C1_Write(uint8_t devAddr, uint8_t* pData, uint8_t len);
I2C_Status_t I2C1_Read(uint8_t devAddr, uint8_t len);
I2C_Status_t I2C1_GetStatus(void);
void I2C1_GetBuffer(uint8_t* dest, uint8_t len);

#endif /* INC_I2C_H_ */
