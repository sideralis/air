/*
 * debug.c
 *
 *  Created on: 22 oct. 2018
 *      Author: gautier
 */
#include "esp_common.h"


void printTaskInfo(void)
{
	int heap_size;
	int stack_size;
	xTaskHandle cTask;
	int taskNb;

	cTask = xTaskGetCurrentTaskHandle();
	taskNb = uxTaskGetNumberOfTasks();
	heap_size = system_get_free_heap_size();
	stack_size= uxTaskGetStackHighWaterMark(NULL);
	os_printf("DBG: task %d %d %d %p %s\n", heap_size, stack_size, taskNb, cTask, ((char *)cTask)+0x34);
}
