#include "sht3x.h"

#include "i2c.h"
#include "stm32f0xx_hal.h" /* HAL_GetTick() */

/* Single shot, high repeatability, clock stretching disabled (datasheet 4.3) */
static const uint8_t MEASURE_COMMAND[2] = {0x24U, 0x00U};

#define MEASUREMENT_TIME_MS 16U /* max. 15.5 ms at high repeatability */
#define TRANSFER_TIMEOUT_MS 10U /* a 6-byte read takes ~0.7 ms at 100 kHz */
#define RETRY_DELAY_MS      100U
#define RESPONSE_LENGTH     6U /* T_msb T_lsb T_crc RH_msb RH_lsb RH_crc */

typedef enum
{
    SHT_IDLE, /* waiting for the next measurement period */
    SHT_SENDING_COMMAND,
    SHT_MEASURING, /* sensor busy, it NACKs reads during this time */
    SHT_READING
} SHT_State_t;

static SHT_State_t s_state = SHT_IDLE;
static uint32_t s_state_tick = 0U;
static uint32_t s_next_measurement_tick = 0U;

static SHT3x_Measurement_t s_measurement;
static bool s_new_measurement = false;
static SHT3x_Stats_t s_stats;

uint8_t SHT3x_Crc8(const uint8_t *data, uint8_t length)
{
    /* CRC-8, polynomial 0x31, init 0xFF (datasheet 4.12) */
    uint8_t crc = 0xFFU;

    for (uint8_t i = 0U; i < length; i++)
    {
        crc ^= data[i];

        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            crc = ((crc & 0x80U) != 0U) ? (uint8_t)((crc << 1U) ^ 0x31U) : (uint8_t)(crc << 1U);
        }
    }

    return crc;
}

float SHT3x_RawToCelsius(uint16_t raw)
{
    return -45.0f + (175.0f * (float)raw / 65535.0f);
}

float SHT3x_RawToRelativeHumidity(uint16_t raw)
{
    return 100.0f * (float)raw / 65535.0f;
}

static void enter(SHT_State_t state, uint32_t now)
{
    s_state = state;
    s_state_tick = now;
}

static void schedule_next(uint32_t now, uint32_t delay_ms)
{
    s_next_measurement_tick = now + delay_ms;
    enter(SHT_IDLE, now);
}

static void fail(uint32_t now, uint32_t *counter)
{
    (*counter)++;
    schedule_next(now, RETRY_DELAY_MS);
}

static void process_response(uint32_t now)
{
    uint8_t data[RESPONSE_LENGTH];

    if (I2C1_CopyReadData(data, RESPONSE_LENGTH) != RESPONSE_LENGTH)
    {
        fail(now, &s_stats.i2c_errors);
        return;
    }

    if ((SHT3x_Crc8(&data[0], 2U) != data[2]) || (SHT3x_Crc8(&data[3], 2U) != data[5]))
    {
        fail(now, &s_stats.crc_errors);
        return;
    }

    const uint16_t raw_temperature = (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
    const uint16_t raw_humidity = (uint16_t)(((uint16_t)data[3] << 8U) | data[4]);

    s_measurement.temperature_c = SHT3x_RawToCelsius(raw_temperature);
    s_measurement.humidity_rh = SHT3x_RawToRelativeHumidity(raw_humidity);
    s_new_measurement = true;
    s_stats.measurements++;

    schedule_next(now, SHT3X_MEASUREMENT_PERIOD_MS);
}

/* Handles the end of an I2C transfer. Returns true when it completed. */
static bool transfer_finished(uint32_t now)
{
    const I2C_Status_t status = I2C1_GetStatus();

    if (status == I2C_COMPLETE)
    {
        return true;
    }

    if (status == I2C_ERROR)
    {
        fail(now, &s_stats.i2c_errors);
    }
    else if ((now - s_state_tick) > TRANSFER_TIMEOUT_MS)
    {
        I2C1_Abort();
        fail(now, &s_stats.timeouts);
    }

    return false;
}

void SHT3x_Update(void)
{
    const uint32_t now = HAL_GetTick();

    switch (s_state)
    {
    case SHT_IDLE:
        /* Signed difference handles HAL tick wrap-around */
        if ((int32_t)(now - s_next_measurement_tick) >= 0)
        {
            if (I2C1_WriteAsync(SHT3X_I2C_ADDRESS, MEASURE_COMMAND, sizeof(MEASURE_COMMAND)))
            {
                enter(SHT_SENDING_COMMAND, now);
            }
        }
        break;

    case SHT_SENDING_COMMAND:
        if (transfer_finished(now))
        {
            enter(SHT_MEASURING, now);
        }
        break;

    case SHT_MEASURING:
        if ((now - s_state_tick) >= MEASUREMENT_TIME_MS)
        {
            if (I2C1_ReadAsync(SHT3X_I2C_ADDRESS, RESPONSE_LENGTH))
            {
                enter(SHT_READING, now);
            }
            else
            {
                fail(now, &s_stats.i2c_errors);
            }
        }
        break;

    case SHT_READING:
        if (transfer_finished(now))
        {
            process_response(now);
        }
        break;

    default:
        schedule_next(now, RETRY_DELAY_MS);
        break;
    }
}

bool SHT3x_GetMeasurement(SHT3x_Measurement_t *measurement)
{
    if ((measurement == NULL) || !s_new_measurement)
    {
        return false;
    }

    *measurement = s_measurement;
    s_new_measurement = false;

    return true;
}

SHT3x_Stats_t SHT3x_GetStats(void)
{
    return s_stats;
}
