/*
 * event.c
 *
 *  Created on: Oct 21, 2019
 *      Author: Mateusz Gawron
 */

#include "main.h"

#include "utils.h"
#include "event.h"
#include <i2c.h>
#include <usart.h>
#include "adc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>


#include <queue.h>

#include "cli.h"



#define REFERENCE_VOLTAGE					4096
#define CURRENT_DISCHARGE_COEFFICIENT		(0.003921f)
#define CURRENT_CALIBRATION_COEFFICIENT		(0.00844f)

#define NUMBER_OF_CELLS						4
#define AVERAGE_VOLTAGE_CAL_NUM_OF_SAMPLE	10

#define CFG_EVENT_QUEUE_SIZE				10

#define EVENT_INIT(_name, _desc, _cb)		\
{											\
	.name = _name,							\
	.desc = _desc,							\
	.cb = _cb,								\
}

#define I2C_ADDRESS 						(0x16)
#define EVENT_TABLE_SIZE					8

#define ADC_REFERENCE_VOLTAGE				2969
#define ADC_OFFSET							2

static QueueHandle_t queue_handle;

typedef void (*event_cb)(void * args);

struct event
{
	const char* name;
	const char* desc;
	event_cb	cb;
};

struct float_bq78350
{
	uint8_t exponent;
	uint8_t sign_bit : 1;
	uint8_t mantissa_lsb : 7;
	uint8_t mantissa_7to15;
	uint8_t mantissa_msb;
}__attribute__((packed));

struct float_struct
{
	uint32_t mantissa : 23;
	uint8_t exponent : 8;
	uint8_t sign_bit : 1;
}__attribute__((packed));

union float_data
{
	float f;
	struct float_struct f_stct;
};

union send_conv
{
	struct float_bq78350 f;
	uint8_t bytes[sizeof(struct float_bq78350)];
};

static void led_on_off(void* args)
{
	if (strcmp(args,"ON") == 0)
	{
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
	}
	else if (strcmp(args,"OFF") == 0)
	{
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);
	}
}

static void read(void* args)
{
	char tmp[7];
	char tmp2[3];
	char type[4];

	memset(tmp, 0, 7);
	memcpy(tmp, args, 6);
	uint16_t reg_addr = hex2int(tmp); /* TODO: check if that works properly */

	memcpy(tmp2, args + 7, 3);
	if (strstr(tmp2," ") != NULL)
		tmp2[2] = '\0';

	uint16_t len = hex2int(tmp2);

	memcpy(type, args + 10, 4);

	uint8_t data[40];
	memset(data, 0, 40);

	if (HAL_I2C_Mem_Read(&hi2c3, I2C_ADDRESS, reg_addr, 1, (uint8_t*)data, len, 1000) != HAL_OK)
	{
		cli_send_string("[I2C] Read ERROR \n\r");
		return;
	}

	memcpy((uint8_t *)args, data, len);

	if (strstr(type, "int") != NULL)
	{
		char tmp_string[60];

		memset(tmp_string, 0, 60);

		uint8_t iterator = 0;
		for (uint8_t i = 0; i < len; i++)
		{
			char c[sizeof(int)];
			itoa(data[i], c, 16);
			strcpy(tmp_string + iterator, c);

			iterator += strlen(c) + 1;
			tmp_string[iterator - 1] = ' ';
		}
		tmp_string[strlen(tmp_string)] = '\n';
		cli_send_string(tmp_string);
	}
	else if (strstr(type, "char") != NULL)
	{
		data[strlen((char*)data)] = '\n';
		cli_send_string((char*)data);
	}
	else
	{
		cli_send_string("Unknown type!\n\r");
	}
}

static void write(void* args)
{

	char reg_str[7];
	memset(reg_str, 0, 7);
	memcpy(reg_str, args, 6);
	uint16_t reg_addr = hex2int(reg_str); /* TODO: check if that works properly */

	char data[90];
	memset(data, 0, 90);
	memcpy(data, args + 7, strlen(args));


	uint8_t hex_data[50];
	memset(hex_data, 0, 50);

	char str_tmp [40];
	itoa(strlen(data) / 2, str_tmp, 10);
	str_tmp[strlen(str_tmp)] = '\n';
	cli_send_string(str_tmp);

	for (uint16_t i = 0; i < strlen(data) / 2; i++)
	{
		char hex_string[2];
		hex_string[0] = data[2 * i];
		hex_string[1] = data[2 * i + 1];
		hex_data[i] = hex2int(hex_string);
	}
	HAL_StatusTypeDef hal_status = HAL_I2C_Mem_Write(&hi2c3, I2C_ADDRESS, reg_addr, 1, hex_data, strlen(data) / 2, 100000);

	if( hal_status != HAL_OK)
		cli_send_string("I2C data send ERROR \n\r");
}

static void adc_fun(void* args)
{
    uint16_t ADCValue;


    HAL_ADC_Start(&hadc1);
    vTaskDelay(2000);
    float average_voltage = 0;

    for(uint8_t i = 0; i < 100; i++)
    {
    	TickType_t start = xTaskGetTickCount();
    	if (HAL_ADC_PollForConversion(&hadc1, 1000) != HAL_OK)
    		cli_send_string("ADC conversion error!\n\r");
        ADCValue = HAL_ADC_GetValue(&hadc1);
        float voltage = ADCValue * ADC_REFERENCE_VOLTAGE / 4095;
        average_voltage += voltage;

        vTaskDelayUntil(&start, 20);
    }

    average_voltage /= 100;
    average_voltage -= ADC_OFFSET;
    uint16_t int_voltage = average_voltage;
//    if(average_voltage - int_voltage > 0.5f)
//    	int_voltage++;

    *(float *)args = average_voltage;
    char tmp[6];

    itoa(int_voltage, tmp, 10);
    strcat(tmp, "\n\r");
    cli_send_string(tmp);

}

static void volt_cal(void* args)
{
	write("0x0000 2d00");
	write("0x0000 81f0");

	uint8_t prev_counter = 0;
	while(true)
	{
		char data[33] = "0x0023 02 int";
		read(data);

		uint8_t counter = (uint8_t)(data[1]);
		if (prev_counter == 0)
			prev_counter = (uint8_t)(data[1]);

		if (counter - prev_counter > 2)
		{
			cli_send_string("counter increments by 2 \n\r");
			break;
		}
	}

	uint8_t measurement_counter = 0;
	prev_counter = 0;

	uint16_t cell_voltage_measurements[NUMBER_OF_CELLS][AVERAGE_VOLTAGE_CAL_NUM_OF_SAMPLE];

	while(true)
	{
		if (measurement_counter >= AVERAGE_VOLTAGE_CAL_NUM_OF_SAMPLE)
			break;
		char data[33] = "0x0023 0D int";
		read(data);

		uint8_t counter = (uint8_t)(data[1]);

		if (prev_counter == 0)
			prev_counter = (uint8_t)(data[1]);

		if (counter - prev_counter >= 1)
		{
			prev_counter = counter;
			for(uint8_t j = 0; j < NUMBER_OF_CELLS; j++)
			{
				cell_voltage_measurements[j][measurement_counter] = (uint8_t)(data[j * 2 + 5]) + 256 * (uint8_t)(data[j * 2 + 6]);
			}
			measurement_counter ++;
		}
	}
	uint32_t cells_voltage_average[NUMBER_OF_CELLS];
	for(uint8_t i = 0; i < NUMBER_OF_CELLS; i++)
	{
		for(uint8_t j = 0; j < AVERAGE_VOLTAGE_CAL_NUM_OF_SAMPLE; j++)
			cells_voltage_average[i] +=  cell_voltage_measurements[i][j];

		cells_voltage_average[i] /= AVERAGE_VOLTAGE_CAL_NUM_OF_SAMPLE;
	}

	int8_t cells_voltage_offset[NUMBER_OF_CELLS];
	bool overflow = false;

	for(uint8_t i = 0; i < NUMBER_OF_CELLS; i++)
	{
		uint16_t tmp_offset = REFERENCE_VOLTAGE - cells_voltage_average[i];
		if (tmp_offset > 0xFF)
		{
			cli_send_string("OFFSET IS TOO BIG\n");
			overflow = true;
		}
		else
		{
			cells_voltage_offset[i] = (int8_t)tmp_offset;
		}
	}

	if (!overflow)
	{
		char offsets[22] = "0x0044 06c040";
		offsets[21] = 0;
		for(uint8_t i = 0; i < NUMBER_OF_CELLS; i++)
			itoa((uint8_t)cells_voltage_offset[i], offsets + 13 + i*2, 16);
		write(offsets);
	}

	write("0x0000 2d00");

	char voltage[33] = "0x0009 02 int";
	read(voltage);

	uint16_t voltage_mv = (uint8_t)(voltage[0]) + (uint8_t)(voltage[1])*256;
	if ((voltage_mv - 4 * REFERENCE_VOLTAGE < 2 ) && (voltage_mv - 4 * REFERENCE_VOLTAGE > -2 ))
		cli_send_string("Voltage calibration success!\n");
	else
		cli_send_string("Voltage calibration fail!\n");

	/*TODO: ADD SAVING THIS DATA TO FLASH MEMORY */
}

static void current_cal(void *args)
{
	/* CC Offset Calibration */

	write("0x0000 2d00");
	write("0x0000 82f0");

	uint8_t prev_counter = 0;
	while(true)
	{
		char data[33] = "0x0023 03 int";
		read(data);

		if((uint8_t)(data[2]) != 2)
		{
			write("0x0000 82f0");
			continue;
		}

		uint8_t counter = (uint8_t)(data[1]);
		if (prev_counter == 0)
			prev_counter = (uint8_t)(data[1]);

		if (counter - prev_counter > 2)
		{
			cli_send_string("counter increments by 2 \n\r");
			break;
		}
	}

	uint8_t measurement_counter = 0;
	int64_t fcal = 0;
	uint8_t current_data[20];


	while(true)
	{
		if (measurement_counter >= AVERAGE_VOLTAGE_CAL_NUM_OF_SAMPLE)
			break;

		char data[33] = "0x0023 11 int";
		read(data);

		if((uint8_t)(data[2]) != 2)
		{
			write("0x0000 82f0");
			continue;
		}

		memcpy(current_data, (uint8_t*)data, 17);
		uint8_t counter = (uint8_t)(data[1]);
		if (counter - prev_counter >= 1)
		{
			prev_counter = counter;
			if((current_data[15] + current_data[16] * 255) < 0x8000)
				fcal += (current_data[15] + current_data[16] * 256);
			else
				fcal += (0xFFFF - current_data[15] - current_data[16] * 256 + 0x0001);

			measurement_counter ++;
		}
	}
	fcal /= AVERAGE_VOLTAGE_CAL_NUM_OF_SAMPLE;
	write("0x0000 1a40");
	char data[33] = "0x0023 03 int";
	read(data);
	uint16_t coulomb_offset = ((uint8_t)data[1] + (uint8_t)data[2] * 256);
	uint16_t cc_offset =  coulomb_offset * fcal;
	/*TODO WRITE THIS OFFSET TO DATAFLASH */

	/* CC Gain / Capacity Gain calibration */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
	write("0x0000 82f0");

	while(true)
	{
		char data[33] = "0x0023 03 int";
		read(data);

		if((uint8_t)(data[2]) != 2)
		{
			write("0x0000 82f0");
			continue;
		}

		uint8_t counter = (uint8_t)(data[1]);
		if (prev_counter == 0)
			prev_counter = (uint8_t)(data[1]);

		if (counter - prev_counter > 5)
		{
			cli_send_string("counter increments by 5 \n\r");
			break;
		}
	}

	write("0x0000 1f00");
	write("0x0000 2000");

	fcal = 0;
	memset(current_data, 0, 20);
	measurement_counter = 0;
	prev_counter = 0;

	while(true)
	{
		if (measurement_counter >= AVERAGE_VOLTAGE_CAL_NUM_OF_SAMPLE)
			break;

		char data[33] = "0x0023 14 int";
		read(data);

		if((uint8_t)(data[2]) != 2)
		{
			write("0x0000 82f0");
			continue;
		}

		memcpy(current_data, (uint8_t*)data, 17);

		uint8_t counter = (uint8_t)(data[1]);
		if (prev_counter == 0)
			prev_counter = (uint8_t)(data[1]);

		if (counter - prev_counter >= 1)
		{
			prev_counter = counter;
			if((current_data[15] + current_data[16] * 256) < 0x8000)
				fcal += (current_data[15] + current_data[16] * 256);
			else
				fcal += (0xFFFF - current_data[15] - current_data[16] * 256 + 0x0001);

			measurement_counter++;
		}
	}

	write("0x0000 1f00");
	write("0x0000 2000");
	float adc_measurement = 0;
	adc_fun(&adc_measurement);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);

    fcal /= AVERAGE_VOLTAGE_CAL_NUM_OF_SAMPLE;
	float icc = adc_measurement * CURRENT_DISCHARGE_COEFFICIENT;

	union float_data cc_gain;
	union float_data capacity_gain;

	cc_gain.f =  icc/(fcal - cc_offset / coulomb_offset) * 1000;
	capacity_gain.f = cc_gain.f * 298261.6178f;
	union send_conv cc_gain_data;
	union send_conv capacity_gain_data;

	cc_gain_data.f.mantissa_lsb = cc_gain.f_stct.mantissa;
	cc_gain_data.f.mantissa_7to15 = cc_gain.f_stct.mantissa >> 7;
	cc_gain_data.f.mantissa_msb = cc_gain.f_stct.mantissa >> 15;

	cc_gain_data.f.exponent = cc_gain.f_stct.exponent;
	cc_gain_data.f.sign_bit = cc_gain.f_stct.sign_bit;

	capacity_gain_data.f.mantissa_lsb = capacity_gain.f_stct.mantissa;
	capacity_gain_data.f.mantissa_7to15 = capacity_gain.f_stct.mantissa >> 7;
	capacity_gain_data.f.mantissa_msb = capacity_gain.f_stct.mantissa >> 15;

	capacity_gain_data.f.exponent = capacity_gain.f_stct.exponent;
	capacity_gain_data.f.sign_bit = capacity_gain.f_stct.sign_bit;


	char offsets[31] = "0x0044 0a0040";
	offsets[30] = 0;
	for(uint8_t i = 0; i < sizeof(struct float_bq78350); i++)
	{
		if (cc_gain_data.bytes[i] <= 0x0F)
		{
			itoa(cc_gain_data.bytes[i], offsets + 13 + i*2 + 1, 16);
			offsets[13 + i * 2] = '0';
		}
		else
			itoa(cc_gain_data.bytes[i], offsets + 13 + i*2, 16);
	}

	for(uint8_t i = 0; i < sizeof(struct float_bq78350); i++)
	{
		if (capacity_gain_data.bytes[i] <= 0x0F)
		{
			itoa(capacity_gain_data.bytes[i], offsets + 13 + sizeof(float) * 2 + i * 2 + 1, 16);
			offsets[13 + sizeof(float) * 2 + i * 2] = '0';
		}
		else
			itoa(capacity_gain_data.bytes[i], offsets + 13 + sizeof(float) * 2 + i * 2, 16);
	}
	write(offsets);
	/*TODO: WRITE; CC GAIN AND CAPACITY GAIN TO FLASH */
	write("0x0000 2d00");
}
static void current_display (void* args)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
    vTaskDelay(6000);
	char data[20] = "0x000a 02 int";
	read(data);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);


	int32_t current_val = (uint8_t)(data[0]) + (uint8_t)(data[1])*256;

	if(current_val > 0x7FFF)
		current_val = -(0xFFFF - current_val + 0x0001);

	char send_dat[10];
	send_dat[9] = '\0';
	itoa(current_val, send_dat, 10);


	cli_send_string(send_dat);
}

static void help_fun(void *args);

const struct event event_tab[EVENT_TABLE_SIZE] =
{
	EVENT_INIT("help", "Show a function description. Argument is a name of a function. \n", help_fun),
	EVENT_INIT("led", "Turn on or off led. Use argument ON to turn on, OFF to turn off. \n", led_on_off),
	EVENT_INIT("read", "Reads data from I2C/SMBus interface. Arguments are: 0xhhhh aa tttt, where hhhh is a register address in hex, aa is a number of data to read, tttt is a type of data. \n", read),
	EVENT_INIT("write", "Writes data to I2C/SMBus interaface. Arguments are: 0xhhhh aaaa.., where hhhh is a register address in hex, aa.. are data in little endian order. \n",  write),
	EVENT_INIT("adc_measurement", "Performs 10 adc measurement and averages result \n", adc_fun),
	EVENT_INIT("voltage_cal", "Performs voltage calibration of bq78350-R1. \n", volt_cal),
	EVENT_INIT("current_cal", "Performs current calibration of bq78350-R1. \n", current_cal),
	EVENT_INIT("display_i", "Displays current. \n", current_display),
};

static void help_fun(void *args)
{
	bool matched = false;
	for(uint8_t i = 0; i < EVENT_TABLE_SIZE; i++)
	{
		if (strstr(args, event_tab[i].name) != NULL)
		{
			cli_send_string(event_tab[i].desc);
			matched = true;
			break;
		}
	}
	if (!matched)
	{
		for(uint8_t i = 1; i < EVENT_TABLE_SIZE; i++)
		{
			uint8_t str_len = strlen(event_tab[i].name);
			char tmp[str_len + 1];
			memset(tmp, 0, str_len + 1);

			memcpy(tmp, event_tab[i].name, str_len);
			cli_send_string(strcat(tmp, "\n\r"));
		}
	}

	char data[4];
	data[3] = '\0';

	itoa(sizeof(struct float_bq78350), data, 10);
	cli_send_string(data);
}

static int16_t seek_4_event(char* text, void* args)
{
	strcpy(args, strstr(text, " ") + 1);

	for(int i = 0; i < EVENT_TABLE_SIZE; i++)
	{
		if (strstr(text, event_tab[i].name) != NULL)
			return i;
	}
	return -1; // error
}
bool event_init(void)
{
	queue_handle = xQueueCreate(CFG_EVENT_QUEUE_SIZE,CFG_DATA_BUFFER_SIZE);

	if(queue_handle == NULL)
		return false;

	return true;
}

bool event_send(char* text)
{
	if (xQueueSend(queue_handle, text, 10) != pdPASS)
		return false;

	return true;
}

void event_process(void)
{
	char text[CFG_DATA_BUFFER_SIZE] ;
	char args[CFG_DATA_BUFFER_SIZE];
	memset(text, 0, CFG_DATA_BUFFER_SIZE);
	memset(args, 0, CFG_DATA_BUFFER_SIZE);

	int16_t id  = 0;

	if (xQueueReceive(queue_handle, text, 0) == pdTRUE)
	{
		id = seek_4_event(text, args);
		if(id >= 0)
			event_tab[id].cb(args);
		else
			cli_send_string("Unknown command!\n\r");
	}
}

