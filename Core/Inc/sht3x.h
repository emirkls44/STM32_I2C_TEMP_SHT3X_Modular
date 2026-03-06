/*
 * sht3x.h
 *
 *  Created on: 6 Mar 2026
 *      Author: Ebrar
 */

#ifndef INC_SHT3X_H_
#define INC_SHT3X_H_

#ifndef SHT3X_H
#define SHT3X_H

#include "i2c.h"

typedef enum {
    SHT_READY_TO_START,
    SHT_SENDING_CMD,
    SHT_MEASURING,
    SHT_READING,
    SHT_DATA_VALID
} SHT_Process_t;

void SHT3x_Update(void);
float SHT3x_GetTemperature(void);
float SHT3x_GetHumidity(void);
int SHT3x_IsDataNew(void);

#endif


#endif /* INC_SHT3X_H_ */
