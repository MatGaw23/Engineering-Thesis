/*
 * cli.c
 *
 *  Created on: Oct 21, 2019
 *  Author: Mateusz Gawron
 */
#include "cli.h"

#include <stdlib.h>
#include <string.h>

#include "event.h"
#include "usart.h"
#include "ring_buffer.h"

#define BACKSPACE_VALUE			0x8
#define NEW_LINE_VALUE			0xa

static char data_buffer[CFG_DATA_BUFFER_SIZE];

void cli_init(void)
{
	memset(data_buffer, 0, CFG_DATA_BUFFER_SIZE);
}

bool cli_send_string(const char* text)
{
	if(text == NULL)
		return false;

	__disable_irq();
	uint16_t len = strlen(text);

	if (HAL_UART_Transmit(&huart1, (uint8_t*)text, len, 1000) == HAL_OK)
	{
		__enable_irq();
		return true;
	}

	__enable_irq();

	return false;
}

void cli_process(void)
{
	uint8_t tmp;
	static uint16_t iterator = 0;
	 if (HAL_UART_Receive(&huart1, &tmp, 1, 5) == HAL_OK)
	 {
		 /* If read symbol is backspace, delete previous character from buffer. */
		 if (tmp == NEW_LINE_VALUE)
		 {
			event_send(data_buffer);
			memset(data_buffer, 0, CFG_DATA_BUFFER_SIZE);
			iterator = 0;
			HAL_UART_Transmit(&huart1, &tmp, 1, 5);
		 }
		 else if (tmp == BACKSPACE_VALUE)
		 {
			 HAL_UART_Transmit(&huart1, &tmp, 1, 5);

			/* If data buffer is not empty remove last character */
			if(strcmp(data_buffer,"") != 0)
			{

				iterator--;
				data_buffer[iterator] = 0;
			}
		 }
		 else
		 {
			 if (iterator >= CFG_DATA_BUFFER_SIZE)
			 {
				 cli_send_string("Buffer overflow!\n");
			 }
			 else
			 {
				 data_buffer[iterator] = tmp;
				 iterator++;
				 HAL_UART_Transmit(&huart1, &tmp, 1, 5);
			 }
		 }
	 }
}
