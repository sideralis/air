/*
 * user_queue.h
 *
 *  Created on: 21 juin 2018
 *      Author: gautier
 */

#ifndef INCLUDE_USER_QUEUE_H_
#define INCLUDE_USER_QUEUE_H_

extern xQueueHandle wifi_scan_queue;
extern xQueueHandle led_queue;
extern xQueueHandle network_queue;
extern xQueueHandle status_scan_queue;
extern xQueueHandle got_ip_queue;

extern xSemaphoreHandle connect_sem;

extern int start_tcpclient;

#endif /* INCLUDE_USER_QUEUE_H_ */
