/*
 * i2c.c
 *
 *  Created on: 6 Mar 2026
 *      Author: Ebrar
 */
#include "i2c.h"

static volatile I2C_Status_t i2c_state = I2C_IDLE;
static uint8_t i2c_rx_buf[10];
static uint8_t rx_idx = 0;
static uint8_t* tx_ptr;
static uint8_t tx_len = 0;

void I2C1_Init(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    GPIOB->MODER |= (GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1); // AF Mode
    GPIOB->OTYPER |= (GPIO_OTYPER_OT_6 | GPIO_OTYPER_OT_7);      // Open Drain
    GPIOB->AFR[0] |= (1 << (6 * 4)) | (1 << (7 * 4));           // AF1

    I2C1->TIMINGR = 0x10420F13;
    I2C1->CR1 |= I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_TCIE | I2C_CR1_NACKIE | I2C_CR1_PE;

    NVIC_EnableIRQ(I2C1_IRQn);
}

I2C_Status_t I2C1_WriteAsync(uint8_t devAddr, uint8_t* pData, uint8_t len) {

    if(i2c_state == I2C_BUSY) {
    	return I2C_BUSY;
    }

    tx_ptr = pData;
    tx_len = len;
    i2c_state = I2C_BUSY;

    I2C1->CR2 = (devAddr << 1) | (len << I2C_CR2_NBYTES_Pos) | I2C_CR2_START;
    return I2C_BUSY;
}

I2C_Status_t I2C1_ReadAsync(uint8_t devAddr, uint8_t len) {

    if(i2c_state == I2C_BUSY) {
    	return I2C_BUSY;
    }

    rx_idx = 0;
    i2c_state = I2C_BUSY;

    I2C1->CR2 = (devAddr << 1) | I2C_CR2_RD_WRN | (len << I2C_CR2_NBYTES_Pos) | I2C_CR2_START;
    return I2C_BUSY;
}

I2C_Status_t I2C1_GetStatus(void) {
	return i2c_state;
}

void I2C1_CopyBuffer(uint8_t* dest, uint8_t len) {

    for(int i=0; i<len; i++)
    	dest[i] = i2c_rx_buf[i];
}

void I2C1_IRQHandler(void) {
    uint32_t isr = I2C1->ISR;

    if (isr & I2C_ISR_TXIS) {
        if(tx_len > 0) {
        	I2C1->TXDR = *tx_ptr++;
        	tx_len--;
        }
    }
    if (isr & I2C_ISR_RXNE) {
        if(rx_idx < 10)
        	i2c_rx_buf[rx_idx++] = I2C1->RXDR;
    }
    if (isr & I2C_ISR_TC) {
        I2C1->CR2 |= I2C_CR2_STOP;
        i2c_state = I2C_COMPLETE;
    }
    if (isr & I2C_ISR_NACKF) {
        I2C1->ICR |= I2C_ICR_NACKCF;
        i2c_state = I2C_ERROR;
    }
}

