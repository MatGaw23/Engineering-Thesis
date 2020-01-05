/*
 * cli.h
 *
 *  Created on: Oct 21, 2019
 *  Author: Mateusz Gawron
 */

#ifndef CLI_H_
#define CLI_H_

#include <stdbool.h>
#define CFG_DATA_BUFFER_SIZE 	100

/* @brief 	Initializes CLI. */
void cli_init(void);

/*
 * @brief	Sends string by uart.
 * @params	text - string which will be transmit by uart.
 * @return	True if everything goes well, otherwise false.
 */
bool cli_send_string(const char* text);

/* @brief	Function that handles receiving data from host computer. */
void cli_process(void);

#endif /* CLI_H_ */
