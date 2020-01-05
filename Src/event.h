/*
 * event.h
 *
 *  Created on: Oct 21, 2019
 *      Author: Mateusz Gawron
 */

#ifndef EVENT_H_
#define EVENT_H_

#include <stdbool.h>

struct event_item
{
	char* name;
	void *args;
};

void event_process(void);

bool event_init(void);

bool event_send(char* text);

#endif /* EVENT_H_ */
