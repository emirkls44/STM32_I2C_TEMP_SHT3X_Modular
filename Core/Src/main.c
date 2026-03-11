#include "i2c.h"
#include "sht3x.h"

float current_t = 0.0f;
float current_h = 0.0f;

int main(void) {
		HAL_Init();
	    I2C1_Init();

    while(1) {

        SHT3x_Update();

        if(SHT3x_IsDataNew()){
            float current_t = SHT3x_GetTemperature();
            float current_h = SHT3x_GetHumidity();
           (void)current_t; //şimdilik kullanmıyorum
           (void)current_h;
        }


    }
}
