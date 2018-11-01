/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include "esp_common.h"
#include "uart.h"

#include "lwip/sockets.h"
#include "lwip/netif.h"

#include "json/cJSON.h"

#include "freertos/queue.h"
#include "freertos/semphr.h"

#ifdef DEBUG
#include "esp8266/ets_sys.h"
#include <../esp-gdbstub/gdbstub.h>
#endif

#include <fcntl.h>

#include "gpio.h"
#include "pwm.h"

#include "user_led.h"
#include "user_queue.h"
#include "user_wifi.h"

/* Defines */
#define AIR_VERSION			"0.0.4 (" __DATE__ " " __TIME__ ")"
#define SERVER_LOCAL_PORT   80

/* Prototypes */
void task_sds011(void *);
void task_led(void *);
void task_wifi_scan(void *);
void task_softap(void *);
void task_station(void *);

void user_tcpserver_init(uint32);
void user_set_softap_config();

void user_mdns_config();

/* Global */
SLIST_HEAD(router_info_head, router_info) router_data;
u8_t mac[NETIF_MAX_HWADDR_LEN];

/* Functions */

/**
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reserved 4 sectors, used for rf init data and parameters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
 */
uint32 user_rf_cal_sector_set(void)
{
	flash_size_map size_map = system_get_flash_size_map();
	uint32 rf_cal_sec = 0;

	switch (size_map) {
	case FLASH_SIZE_4M_MAP_256_256:
		rf_cal_sec = 128 - 5;
		break;

	case FLASH_SIZE_8M_MAP_512_512:
		rf_cal_sec = 256 - 5;
		break;

	case FLASH_SIZE_16M_MAP_512_512:
	case FLASH_SIZE_16M_MAP_1024_1024:
		rf_cal_sec = 512 - 5;
		break;

	case FLASH_SIZE_32M_MAP_512_512:
	case FLASH_SIZE_32M_MAP_1024_1024:
		rf_cal_sec = 1024 - 5;
		break;
	case FLASH_SIZE_64M_MAP_1024_1024:
		rf_cal_sec = 2048 - 5;
		break;
	case FLASH_SIZE_128M_MAP_1024_1024:
		rf_cal_sec = 4096 - 5;
		break;
	default:
		rf_cal_sec = 0;
		break;
	}

	return rf_cal_sec;
}
/**
 * Main task.
 * It will connect to router, scan networks, ...
 *
 * @param param not used
 */
void task_main(void *param)
{
	signed portBASE_TYPE ret;
	bool ret2;
	int got_ip;
	int nb_ap;								// Nb of router
	struct station_config *config;			// Information on these ap
	struct led_info led_setup;
	struct station_config station_info;

	// Let's blink while we are not fully connected
	led_setup.color_to = LED_WHITE;
	led_setup.color_from = LED_BLACK;
	led_setup.state = LED_BLINK;
	xQueueSend(led_queue, &led_setup, 0);

	// Register wifi call back function
	wifi_set_event_handler_cb(wifi_handle_event_cb);

	config = calloc(5, sizeof(struct station_config));
	if (config == 0) {
		os_printf("ERR: malloc %d %s\n", __LINE__, __FILE__);
		return;
	}
	while (1) {
		// Get ap info
		// wifi_station_get_current_ap_id();
		nb_ap = wifi_station_get_ap_info(config);					// Get number of registered access points
		if (nb_ap == 0) {
			// We have never connected to any network, let's scan for wifi
			// Start task wifi scan
			xTaskCreate(task_wifi_scan, "Scan Wifi Around", 256, NULL, 2, NULL);
			// Wait for end of scan
			ret = xQueueReceive(wifi_scan_queue, &router_data, 10000 / portTICK_RATE_MS);			// Timeout after 10s
			if (ret == errQUEUE_EMPTY) {
				// No wifi detected
				os_printf("INFO: No wifi dectected! Aborting!\n");									// TODO we should do something if there is no wifi
			} else {
				os_printf("INFO: Wifi networks dectected!\n");

				// Switch to SoftAP mode
				xTaskCreate(task_softap, "softap_task", 500, NULL, 6, NULL);
				vTaskDelay(1000 / portTICK_RATE_MS);		// Wait 1s

				// Start TCP server
				os_printf("INFO: Starting TCP server\n");
				start_tcpclient = false;															// TODO replace by some FreeRTOS concept ?
				user_tcpserver_init(SERVER_LOCAL_PORT);

				// User is now supposed to connect to our network and select an access point
				// Wait for user network selection
				ret = xQueueReceive(network_queue, &station_info, portMAX_DELAY);					// station_info contains the name and password of wifi network

				// Switch back to station mode
				xTaskCreate(task_station, "station_task", 500, NULL, 6, NULL);
				vTaskDelay(1000 / portTICK_RATE_MS);		// Wait 1s

				// User has now selected network, let's try to connect to it:
				// Info on the selected network is in station_info variable
				convert_UTF8_string(station_info.ssid);
				convert_UTF8_string(station_info.password);
				os_printf("INFO: After UTF8 conversion - Trying to connect to: %s with password: %s\n", station_info.ssid, station_info.password);

				ret2 = wifi_station_set_config(&station_info);
				if (ret2 == false)
					os_printf("ERR: Can not set station config!\n");

				if (!wifi_station_dhcpc_status()) {
					os_printf("ERR: DHCP is not started. Starting it...\n");
					if (!wifi_station_dhcpc_start()) {
						os_printf("ERR: DHCP start failed!\n");
					}
				}

				ret2 = wifi_station_connect();
				if (ret2 == false)
					os_printf("ERR: Can not connect to AP!\n");

				// Wait for connection and ip
				ret = xQueueReceive(got_ip_queue, &got_ip, portMAX_DELAY);
				os_printf("INFO: We should be connected now\n");

				// Switch off led to indicate we are connected to station
				led_setup.color_to = LED_WHITE;
				led_setup.state = LED_OFF;
				xQueueSend(led_queue, &led_setup, 0);

				// Register the device to the user
				os_printf("INFO: Send message to register device\n");
				start_tcpclient = true;								// FIXME replace by something stronger
				tcpserver_disconnect_and_tcpclient_connect();		// TODO process return message

			}
		} else {
			os_printf("DBG: We have already connected to a network. Trying again...\n");

			// Wait for connection and ip
			ret = xQueueReceive(got_ip_queue, &got_ip, portMAX_DELAY);
			os_printf("DBG: We should be connected now\n");

			// Stop blinking to indicate we are connected
			led_setup.color_to = LED_BLUE;
			led_setup.state = LED_ON;
			xQueueSend(led_queue, &led_setup, 0);

		}
		vTaskSuspend(xTaskGetCurrentTaskHandle());
	}
}
/**
 * Display device information as SDK version, chip ID, MAC address
 * Fill the global variable mac with MAC address
 */
static void user_display_device_data(void)
{
	bool ret;

	// Display product information
	ret = wifi_get_macaddr(STATION_IF, mac);
	if (ret == false)
		os_printf("ERR: Can not get MAC address!\n");

	os_printf("INFO: SDK version:%s\n", system_get_sdk_version());
	os_printf("INFO: ESP8266 chip ID:0x%x\n", system_get_chip_id());
	os_printf("INFO: AIR version: " AIR_VERSION " \n");
	os_printf("INFO: MAC address: %0x:%0x:%0x:%0x:%0x:%0x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

}
#ifdef TEST
/**
 * Unit tests
 */
static void user_test(void)
{
	os_printf("\n");
	os_printf("~~~~~~~~~~~~~~~~~~~~~~\n");
	os_printf("    TESTS STARTING\n");

	test_header_html_post1();
	test_header_html_post2();
	test_header_html_get1();
	test_header_html_get2();
	test_spiffs();

	os_printf("    TESTS COMPLETED\n");
	os_printf("~~~~~~~~~~~~~~~~~~~~~~\n");

}
#endif
/**
 * Entry point of the program
 */
void IRAM_ATTR user_init(void)
{

	int led_type = LED_TYPE_RGB;
	bool ret;
	int i;

	// Reconfigure UART to 115200 bauds
	uart_init_new();

#ifdef DEBUG
//	gdbstub_init();							// Uncomment to enable debugging
#endif

#ifdef TEST
	user_test();
	return;
#endif

	// Display id, build number, mac address, ...
	user_display_device_data();

	// Initialize spiffs
	user_spiffs();

	// Create queues
	user_create_queues();

	// Start STATIONAP mode for scanning and for connection
//	xTaskCreate(softap_task,"softap_task",500,NULL,6,NULL);
	xTaskCreate(task_station, "station mode", 500, NULL, 6, NULL);

	// Init to be able to start TCP server later
	espconn_init();

	// Start task led
//	xTaskCreate(task_led, "led driver", 256, &led_type, 2, NULL);

	// Main task - state machine
	xTaskCreate(task_main, "main", 1024, NULL, 2, NULL);

	// Start task sds011
//	xTaskCreate(task_sds011, "sds011 driver", 256, NULL, 2, NULL);

}

