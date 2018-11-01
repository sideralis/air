/*
 * user_queue.c
 *
 *  Created on: 21 juin 2018
 *      Author: gautier
 */

#include "esp_common.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "user_led.h"
#include "user_wifi.h"

/* Global queues */
xQueueHandle wifi_scan_queue; 			/* Queue used to store wifi scan results */
xQueueHandle led_queue; 				/* Queue used to setup led */
xQueueHandle network_queue; 			/* Queue used to tell which wifi network to use */
xQueueHandle status_scan_queue; 		/* Queue used to stop or restart wifi scan */
xQueueHandle got_ip_queue; 				/* Queue used to indicate that ESP8266 station is now connected to an AP and got an IP */

xSemaphoreHandle connect_sem; 			/* Semaphore used to avoid reading and writing at same time file connect_status.json */

int start_tcpclient;					// FIXME to be replaced
/**
 * Create queues
 */
void user_create_queues(void)
{
	// FIXME: check return values

	wifi_scan_queue = xQueueCreate(1, sizeof(SLIST_HEAD(router_info_head, router_info)));
	led_queue = xQueueCreate(1, sizeof(struct led_info));
	network_queue = xQueueCreate(1, sizeof(struct station_config));
	status_scan_queue = xQueueCreate(1, sizeof(int));
	got_ip_queue = xQueueCreate(1, sizeof(int));

	connect_sem = xSemaphoreCreateMutex();
}
