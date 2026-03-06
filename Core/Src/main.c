#include "i2c.h"
#include "sht3x.h"

int main(void) {

    I2C1_Init();

    while(1) {

        SHT3x_Update();

        if(SHT3x_IsDataNew()){
            float current_t = SHT3x_GetTemperature();
            (void)current_t; //şimdilik kullanmıyorum
        }


    }
}
