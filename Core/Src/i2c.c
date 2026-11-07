#include "i2c.h"

#include <string.h>

/* 100 kHz Standard-mode with an 8 MHz I2CCLK (RM0091, I2C timing examples):
 * PRESC = 1, SCLDEL = 4, SDADEL = 2, SCLH = 0x0F, SCLL = 0x13 */
#define I2C_TIMING_100KHZ_8MHZ 0x10420F13U

#define I2C1_PIN_AF 1U /* AF1 = I2C1 on PB6/PB7 */

/* State shared with the interrupt handler */
static volatile I2C_Status_t s_status = I2C_IDLE;
static volatile uint8_t s_tx_buffer[I2C1_MAX_WRITE_LENGTH];
static volatile uint8_t s_tx_length = 0U;
static volatile uint8_t s_tx_index = 0U;
static volatile uint8_t s_rx_buffer[I2C1_MAX_READ_LENGTH];
static volatile uint8_t s_rx_length = 0U;
static volatile uint8_t s_rx_index = 0U;

void I2C1_Init(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* PB6/PB7: alternate function 1, open-drain, high speed */
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODER6 | GPIO_MODER_MODER7)) |
                   (GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1);
    GPIOB->OTYPER |= GPIO_OTYPER_OT_6 | GPIO_OTYPER_OT_7;
    GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR6 | GPIO_OSPEEDR_OSPEEDR7;
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(GPIO_AFRL_AFSEL6 | GPIO_AFRL_AFSEL7)) |
                    (I2C1_PIN_AF << GPIO_AFRL_AFSEL6_Pos) | (I2C1_PIN_AF << GPIO_AFRL_AFSEL7_Pos);

    /* TIMINGR may only be written while the peripheral is disabled */
    I2C1->CR1 &= ~I2C_CR1_PE;
    I2C1->TIMINGR = I2C_TIMING_100KHZ_8MHZ;
    I2C1->CR1 =
        I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_TCIE | I2C_CR1_NACKIE | I2C_CR1_ERRIE | I2C_CR1_PE;

    NVIC_EnableIRQ(I2C1_IRQn);
}

bool I2C1_WriteAsync(uint8_t address_7bit, const uint8_t *data, uint8_t length)
{
    if ((s_status == I2C_BUSY) || (data == NULL) || (length == 0U) ||
        (length > I2C1_MAX_WRITE_LENGTH))
    {
        return false;
    }

    for (uint8_t i = 0U; i < length; i++)
    {
        s_tx_buffer[i] = data[i];
    }
    s_tx_length = length;
    s_tx_index = 0U;
    s_status = I2C_BUSY;

    I2C1->CR2 =
        ((uint32_t)address_7bit << 1U) | ((uint32_t)length << I2C_CR2_NBYTES_Pos) | I2C_CR2_START;

    return true;
}

bool I2C1_ReadAsync(uint8_t address_7bit, uint8_t length)
{
    if ((s_status == I2C_BUSY) || (length == 0U) || (length > I2C1_MAX_READ_LENGTH))
    {
        return false;
    }

    s_rx_length = length;
    s_rx_index = 0U;
    s_status = I2C_BUSY;

    I2C1->CR2 = ((uint32_t)address_7bit << 1U) | I2C_CR2_RD_WRN |
                ((uint32_t)length << I2C_CR2_NBYTES_Pos) | I2C_CR2_START;

    return true;
}

I2C_Status_t I2C1_GetStatus(void)
{
    return s_status;
}

uint8_t I2C1_CopyReadData(uint8_t *destination, uint8_t length)
{
    if (destination == NULL)
    {
        return 0U;
    }

    const uint8_t available = s_rx_index;
    const uint8_t count = (length < available) ? length : available;

    for (uint8_t i = 0U; i < count; i++)
    {
        destination[i] = s_rx_buffer[i];
    }

    return count;
}

void I2C1_Abort(void)
{
    NVIC_DisableIRQ(I2C1_IRQn);

    /* Clearing PE resets the I2C state machine and releases SCL/SDA; it
     * must stay low for at least 3 APB clock cycles (RM0091). */
    I2C1->CR1 &= ~I2C_CR1_PE;
    while ((I2C1->CR1 & I2C_CR1_PE) != 0U)
    {
    }
    __NOP();
    __NOP();
    __NOP();
    I2C1->CR1 |= I2C_CR1_PE;

    s_status = I2C_IDLE;
    NVIC_EnableIRQ(I2C1_IRQn);
}

void I2C1_IRQHandler(void)
{
    const uint32_t isr = I2C1->ISR;

    if ((isr & I2C_ISR_TXIS) != 0U)
    {
        /* Always write TXDR on TXIS, otherwise the interrupt keeps firing */
        I2C1->TXDR = (s_tx_index < s_tx_length) ? s_tx_buffer[s_tx_index++] : 0U;
    }

    if ((isr & I2C_ISR_RXNE) != 0U)
    {
        const uint8_t byte = (uint8_t)I2C1->RXDR; /* reading clears RXNE */

        if (s_rx_index < s_rx_length)
        {
            s_rx_buffer[s_rx_index++] = byte;
        }
    }

    if ((isr & I2C_ISR_NACKF) != 0U)
    {
        /* The hardware sends STOP automatically after a NACK in master mode */
        I2C1->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF;
        s_status = I2C_ERROR;
        return;
    }

    if ((isr & (I2C_ISR_BERR | I2C_ISR_ARLO | I2C_ISR_OVR)) != 0U)
    {
        I2C1->ICR = I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF;
        s_status = I2C_ERROR;
        return;
    }

    if ((isr & I2C_ISR_TC) != 0U)
    {
        /* TC is cleared by generating the STOP condition */
        I2C1->CR2 |= I2C_CR2_STOP;
        s_status = I2C_COMPLETE;
    }

    if ((isr & I2C_ISR_STOPF) != 0U)
    {
        I2C1->ICR = I2C_ICR_STOPCF;
    }
}
