/*
 * Host unit tests for the SHT3x driver (Core/Src/sht3x.c). The I2C driver
 * and HAL tick are replaced by the fakes below, so the state machine can be
 * stepped through NACKs, CRC errors and stuck transfers.
 */
#include "i2c.h"
#include "sht3x.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_failures = 0;

#define CHECK(condition)                                                                           \
    do                                                                                             \
    {                                                                                              \
        if (!(condition))                                                                          \
        {                                                                                          \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                          \
            g_failures++;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_NEAR(a, b) CHECK(fabsf((a) - (b)) < 0.01f)

/* ---- fakes ---------------------------------------------------------- */
static uint32_t g_tick = 0;
static I2C_Status_t g_status = I2C_IDLE;
static int g_writes = 0;
static int g_reads = 0;
static int g_aborts = 0;
static uint8_t g_written[I2C1_MAX_WRITE_LENGTH];
static uint8_t g_read_data[I2C1_MAX_READ_LENGTH];

uint32_t HAL_GetTick(void)
{
    return g_tick;
}

bool I2C1_WriteAsync(uint8_t address_7bit, const uint8_t *data, uint8_t length)
{
    CHECK(address_7bit == SHT3X_I2C_ADDRESS);
    CHECK(length == 2);
    memcpy(g_written, data, length);
    g_writes++;
    g_status = I2C_BUSY;
    return true;
}

bool I2C1_ReadAsync(uint8_t address_7bit, uint8_t length)
{
    CHECK(address_7bit == SHT3X_I2C_ADDRESS);
    CHECK(length == 6);
    g_reads++;
    g_status = I2C_BUSY;
    return true;
}

I2C_Status_t I2C1_GetStatus(void)
{
    return g_status;
}

uint8_t I2C1_CopyReadData(uint8_t *destination, uint8_t length)
{
    memcpy(destination, g_read_data, length);
    return length;
}

void I2C1_Abort(void)
{
    g_aborts++;
    g_status = I2C_IDLE;
}

/* ---- helpers -------------------------------------------------------- */
static void advance(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        g_tick++;
        SHT3x_Update();
    }
}

static void set_response(uint16_t raw_t, uint16_t raw_rh)
{
    g_read_data[0] = (uint8_t)(raw_t >> 8);
    g_read_data[1] = (uint8_t)raw_t;
    g_read_data[2] = SHT3x_Crc8(&g_read_data[0], 2);
    g_read_data[3] = (uint8_t)(raw_rh >> 8);
    g_read_data[4] = (uint8_t)raw_rh;
    g_read_data[5] = SHT3x_Crc8(&g_read_data[3], 2);
}

/* Runs one full measurement cycle that succeeds on the bus. */
static void complete_cycle(void)
{
    const int writes = g_writes;
    while (g_writes == writes)
    {
        advance(1);
    }
    g_status = I2C_COMPLETE; /* command sent */

    const int reads = g_reads;
    while (g_reads == reads)
    {
        advance(1);
    }
    g_status = I2C_COMPLETE; /* data received */
    advance(1);
}

/* ---- tests ---------------------------------------------------------- */
static void test_crc_datasheet_example(void)
{
    const uint8_t data[2] = {0xBE, 0xEF};
    CHECK(SHT3x_Crc8(data, 2) == 0x92);
}

static void test_conversion(void)
{
    CHECK_NEAR(SHT3x_RawToCelsius(0), -45.0f);
    CHECK_NEAR(SHT3x_RawToCelsius(65535), 130.0f);
    CHECK_NEAR(SHT3x_RawToCelsius(0x6666), 25.0f);
    CHECK_NEAR(SHT3x_RawToRelativeHumidity(0), 0.0f);
    CHECK_NEAR(SHT3x_RawToRelativeHumidity(65535), 100.0f);
}

static void test_measurement_cycle(void)
{
    SHT3x_Measurement_t m;

    set_response(0x6666, 0x8000);
    complete_cycle();

    CHECK(g_written[0] == 0x24 && g_written[1] == 0x00);
    CHECK(SHT3x_GetMeasurement(&m));
    CHECK_NEAR(m.temperature_c, 25.0f);
    CHECK_NEAR(m.humidity_rh, 50.0f);
    CHECK(!SHT3x_GetMeasurement(&m)); /* consumed */
}

static void test_waits_for_conversion_before_reading(void)
{
    const int reads = g_reads;
    const int writes = g_writes;
    while (g_writes == writes)
    {
        advance(1);
    }
    g_status = I2C_COMPLETE;
    advance(1); /* -> measuring */

    advance(14);
    CHECK(g_reads == reads); /* 15.5 ms conversion not finished yet */
    advance(2);
    CHECK(g_reads == reads + 1);

    g_status = I2C_COMPLETE;
    advance(1);
}

static void test_measurement_period_limits_self_heating(void)
{
    const int writes = g_writes;
    advance(SHT3X_MEASUREMENT_PERIOD_MS - 2);
    CHECK(g_writes == writes);
    advance(2);
    CHECK(g_writes == writes + 1);

    g_status = I2C_COMPLETE;
    advance(20);
    g_status = I2C_COMPLETE;
    advance(1);
}

static void test_nack_is_retried_not_stuck(void)
{
    const SHT3x_Stats_t before = SHT3x_GetStats();
    const int writes = g_writes;

    while (g_writes == writes)
    {
        advance(1);
    }
    g_status = I2C_ERROR; /* sensor missing: address NACK */
    advance(1);
    CHECK(SHT3x_GetStats().i2c_errors == before.i2c_errors + 1);

    g_status = I2C_IDLE;
    advance(101);
    CHECK(g_writes == writes + 2); /* retried after ~100 ms */

    g_status = I2C_COMPLETE;
    advance(20);
    g_status = I2C_COMPLETE;
    advance(1);
}

static void test_crc_error_rejects_data(void)
{
    SHT3x_Measurement_t m;
    (void)SHT3x_GetMeasurement(&m);
    const SHT3x_Stats_t before = SHT3x_GetStats();

    set_response(0x6666, 0x8000);
    g_read_data[1] ^= 0x01; /* corrupt temperature */
    complete_cycle();

    CHECK(!SHT3x_GetMeasurement(&m));
    CHECK(SHT3x_GetStats().crc_errors == before.crc_errors + 1);
}

static void test_stuck_transfer_is_aborted(void)
{
    const SHT3x_Stats_t before = SHT3x_GetStats();
    const int aborts = g_aborts;
    const int writes = g_writes;

    while (g_writes == writes)
    {
        advance(1);
    }
    /* status stays I2C_BUSY: e.g. SDA held low */
    advance(20);

    CHECK(g_aborts == aborts + 1);
    CHECK(SHT3x_GetStats().timeouts == before.timeouts + 1);

    /* and the driver recovers */
    set_response(0x6666, 0x8000);
    complete_cycle();
    SHT3x_Measurement_t m;
    CHECK(SHT3x_GetMeasurement(&m));
}

int main(void)
{
    struct
    {
        const char *name;
        void (*run)(void);
    } const tests[] = {
        /* The driver keeps static state, so the order matters. */
        {"crc_datasheet_example", test_crc_datasheet_example},
        {"conversion", test_conversion},
        {"measurement_cycle", test_measurement_cycle},
        {"waits_for_conversion_before_reading", test_waits_for_conversion_before_reading},
        {"measurement_period_limits_self_heating", test_measurement_period_limits_self_heating},
        {"nack_is_retried_not_stuck", test_nack_is_retried_not_stuck},
        {"crc_error_rejects_data", test_crc_error_rejects_data},
        {"stuck_transfer_is_aborted", test_stuck_transfer_is_aborted},
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        const int before = g_failures;
        tests[i].run();
        printf("%s %s\n", (g_failures == before) ? "PASS" : "FAIL", tests[i].name);
    }

    printf("\n%d failure(s)\n", g_failures);
    return (g_failures == 0) ? 0 : 1;
}
