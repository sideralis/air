/*
 * user_queue.c
 *
 *  Created on: 21 juin 2018
 *      Author: gautier
 */

#include "esp_common.h"
#include "freertos/queue.h"

#include "user_led.h"
#include "user_wifi.h"

/* Global queues */
xQueueHandle wifi_scan_queue;				/* Queue used to store wifi scan results */
xQueueHandle led_queue;						/* Queue used to setup led */
xQueueHandle network_queue;					/* Queue used to tell which wifi network to use */
xQueueHandle status_scan_queue;				/* Queue used to stop or restart wifi scan */

/**
 * Create queues
 */
void user_create_queues(void)
{
	wifi_scan_queue = xQueueCreate( 1, sizeof(SLIST_HEAD(router_info_head, router_info)) );
	led_queue = xQueueCreate(1, sizeof(struct led_info) );
	network_queue = xQueueCreate(1, sizeof(struct station_config));
	status_scan_queue = xQueueCreate(1, sizeof(int));
}
