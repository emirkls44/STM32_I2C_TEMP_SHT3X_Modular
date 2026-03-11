/*
 * sht3x.c
 *
 *  Created on: 6 Mar 2026
 *      Author: Ebrar
 */

#include "sht3x.h"
#include "i2c.h"

static SHT_Process_t sht_state = SHT_READY_TO_START;
static float _temp = 0, _hum = 0;
static int _is_new_data = 0;
static volatile uint32_t start_time = 0;

void SHT3x_Update(void) {

    uint8_t cmd[] = {0x24, 0x00};
    uint8_t raw_data[6];

    switch(sht_state) {
        case SHT_READY_TO_START:
            if(I2C1_WriteAsync(0x44, cmd, 2) == I2C_BUSY) {
                sht_state = SHT_SENDING_CMD;
            }
            break;

        case SHT_SENDING_CMD:
            if(I2C1_GetStatus() == I2C_COMPLETE) {
            	start_time = HAL_GetTick();
                sht_state = SHT_MEASURING;
            }
            break;

        case SHT_MEASURING:
            if((HAL_GetTick() - start_time) >= 50) {
                I2C1_ReadAsync(0x44, 6);
                sht_state = SHT_READING;
            }
            break;

        case SHT_READING:
            if(I2C1_GetStatus() == I2C_COMPLETE) {
                I2C1_CopyBuffer(raw_data, 6);
                _temp = -45.0f + 175.0f * (float)((raw_data[0]<<8)|raw_data[1]) / 65535.0f;
                _hum = 100.0f * (float)((raw_data[3]<<8)|raw_data[4]) / 65535.0f;
                _is_new_data = 1;
                sht_state = SHT_READY_TO_START;
            } else if(I2C1_GetStatus() == I2C_ERROR){
                sht_state = SHT_READY_TO_START;
            }
            break;

        default: break;
    }
}

float SHT3x_GetTemperature(void){
	_is_new_data = 0;
	return _temp;
}

float SHT3x_GetHumidity(void){
	return _hum;
}

int SHT3x_IsDataNew(void){
	return _is_new_data;
}






