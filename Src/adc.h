/*
 * adc.h
 *
 *  Created on: Dec 19, 2019
 *      Author: Mateusz
 */

#ifndef ADC_H_
#define ADC_H_

#include "main.h"
#include <stm32f4xx_hal_adc.h>

ADC_HandleTypeDef hadc1;

void MX_ADC1_Init(void);


#endif /* ADC_H_ */
