#ifndef SHT3X_H
#define SHT3X_H

/*
 * Non-blocking driver for the Sensirion SHT3x temperature/humidity sensor
 * (SHT30/SHT31/SHT35) on I2C1.
 *
 * Call SHT3x_Update() continuously from the main loop. It starts a
 * single-shot measurement every SHT3X_MEASUREMENT_PERIOD_MS, waits for the
 * conversion without blocking, reads the result, checks both CRCs and
 * recovers from NACKs and stuck transfers.
 */

#include <stdbool.h>
#include <stdint.h>

#define SHT3X_I2C_ADDRESS 0x44U /* ADDR pin low; 0x45 when high */

/* One measurement per second keeps the sensor's self-heating negligible
 * (Sensirion recommends at most one high-repeatability measurement per
 * second). */
#define SHT3X_MEASUREMENT_PERIOD_MS 1000U

typedef struct
{
    float temperature_c;
    float humidity_rh;
} SHT3x_Measurement_t;

typedef struct
{
    uint32_t measurements; /* valid measurements         */
    uint32_t i2c_errors;   /* NACK, bus error, arbitration */
    uint32_t crc_errors;   /* data received but corrupted  */
    uint32_t timeouts;     /* transfer did not finish      */
} SHT3x_Stats_t;

void SHT3x_Update(void);

/* Returns true once for every new valid measurement. */
bool SHT3x_GetMeasurement(SHT3x_Measurement_t *measurement);

SHT3x_Stats_t SHT3x_GetStats(void);

/* Data conversion, exposed for unit tests */
uint8_t SHT3x_Crc8(const uint8_t *data, uint8_t length);
float SHT3x_RawToCelsius(uint16_t raw);
float SHT3x_RawToRelativeHumidity(uint16_t raw);

#endif /* SHT3X_H */
