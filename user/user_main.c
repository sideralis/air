/*
 * ESPRSSIF MIT License
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
#define AIR_VERSION			"0.0.2"
#define SERVER_LOCAL_PORT   80

/* Prototypes */
void task_sds011(void *);
void task_led(void *);
void task_wifi_scan(void *);
void softap_task(void *);
void station_task(void *);

void user_tcpserver_init(uint32 );
void user_set_softap_config();

void user_mdns_config();

/* Global */
SLIST_HEAD(router_info_head, router_info) router_data;

/* Functions */

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and parameters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
 *******************************************************************************/
uint32 user_rf_cal_sector_set(void) {
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

void task_main(void *param)
{
	bool ret2;
	int got_ip;
	int nb_ap;								// Nb of ap air can connect to
	struct station_config config[5];		// Information on these ap
	struct led_info led_setup;
	signed portBASE_TYPE ret;
	struct bss_info *wifi_scan;
	xTaskHandle handle_task_wifi;
	struct station_config station_info;

    struct router_info *info = NULL;

	// Let's blink while we are not fully connected
	led_setup.color_to = LED_WHITE;
	led_setup.color_from = LED_BLACK;
	led_setup.state = LED_BLINK;
	xQueueSend(led_queue, &led_setup, 0);
	wifi_set_event_handler_cb(wifi_handle_event_cb);

	//wifi_station_disconnect();

	while(1) {
		// Get ap info
		//wifi_station_get_current_ap_id();
		nb_ap = wifi_station_get_ap_info(config);
		printf("DBG: Data on station = %d\n", nb_ap);
		if (nb_ap < 500) {		//  FIXME should be == 0
			// We have never connected to any network, let's scan for wifi
			// Start task wifi scan
			xTaskCreate(task_wifi_scan, "Scan Wifi Around", 256, NULL, 2, &handle_task_wifi);

			ret = xQueueReceive(wifi_scan_queue, &router_data, 10000 / portTICK_RATE_MS);			// Timeout after 10s
			if (ret == errQUEUE_EMPTY) {
				os_printf("DBG: No wifi dectected! Aborting!\n");
				// No wifi detected
			} else {
				os_printf("DBG: Wifi dectected!\n");

				// Switch to SoftAP mode
				xTaskCreate(softap_task,"softap_task",500,NULL,6,NULL);
				vTaskDelay(1000 / portTICK_RATE_MS);		// Wait 1s

				// Start TCP server
				os_printf("DBG: Start TCP server\n");
				user_tcpserver_init(SERVER_LOCAL_PORT);

				// Wait for user network selection
				ret = xQueueReceive(network_queue, &station_info, portMAX_DELAY);

				// Switch back to station mode
				xTaskCreate(station_task,"station_task",500,NULL,6,NULL);
				vTaskDelay(1000 / portTICK_RATE_MS);		// Wait 1s

			    // User has now selected network, let's try to connect to it:
				convert_UTF8_string(station_info.ssid);
				convert_UTF8_string(station_info.password);
				os_printf("DBG: After UTF8 conversion - Trying to connect to: %s with password: %s\n",station_info.ssid, station_info.password);

				ret2 = wifi_station_set_config(&station_info);
				if (ret2 == false)
					os_printf("ERR: Can not set station config!\n");

			    if(!wifi_station_dhcpc_status()){
			        os_printf("ERR: DHCP is not started. Starting it...\n");
			        if(!wifi_station_dhcpc_start()){
			            os_printf("ERR: DHCP start failed!\n");
			        }
			    }

				ret2 = wifi_station_connect();
				if (ret2 == false)
					os_printf("ERR: Can not connect to AP!\n");

				// Wait for connection and ip
				ret = xQueueReceive(got_ip_queue, &got_ip, portMAX_DELAY);
				os_printf("DBG: We should be connected now\n");

				// Write file
				char *buf;
				cJSON *connect_msg = cJSON_CreateObject();				// FIXME check if wifi_list == NULL
				cJSON *status = cJSON_CreateString("connected");
				cJSON_AddItemToObject(connect_msg, "status", status);
				buf = cJSON_Print(connect_msg);

				os_printf("DBG: cJSON message:\n%s\n",buf);

				xSemaphoreTake( connect_sem, portMAX_DELAY );
				os_printf("DBG: Start writing status_connect.json \n");
				int pfd;
				pfd = open("/status_connect.json", O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);			// TODO file must be also deleted
				if (pfd <= 3) {
					printf("ERR: open file error!\n");
				}
				int write_byte = write(pfd, buf, strlen(buf));
				if (write_byte <= 0) {
					printf("ERR: write file error (status_connect.json) %d\n",write_byte);
				}
				close(pfd);
				os_printf("DBG: End writing status_connect.json \n");
				xSemaphoreGive( connect_sem );

				free(buf);

				// Switch off led to indicate we are connected to station
				led_setup.color_to = LED_WHITE;
				led_setup.state = LED_OFF;
				xQueueSend(led_queue, &led_setup, 0);

				// Display data

			}
		} else {
			printf("DBG: We have already connected to a network. Trying again...\n");

			// Wait for connection and ip before going to mDNS
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
 * mDNS configuration
 * Not yet working!!!
 */
//void IRAM_ATTR user_mdns_config()
//{
//	struct ip_info ipconfig;
//	struct mdns_info *info;
//
//	wifi_get_ip_info(STATION_IF, &ipconfig);
//
//	if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {
//		os_printf("DBG: setting up mdns\n");
//		info = (struct mdns_info *)zalloc(sizeof(struct mdns_info));
//		if (info == 0)
//			return;
//
//		info->host_name = "air";
//		info->ipAddr = ipconfig.ip.addr;			// ESP8266 Station IP
//		info->server_name = "iot";
//		info->server_port = 80;
//		info->txt_data[0] = "version = now";
//		info->txt_data[1] = "user1 = data1";
//		info->txt_data[2] = "user2 = data2";
//
//		espconn_mdns_init(info);
//	}
//}
/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void IRAM_ATTR user_init(void) {

	int led_type = LED_TYPE_RGB;
	bool ret;
	u8_t mac[NETIF_MAX_HWADDR_LEN];
	u64 mac_full;
	int i;

	// Reconfigure UART to 115200 bauds
	uart_init_new();

#ifdef DEBUG
//	gdbstub_init();
#endif

	// Display product information
	ret = wifi_get_macaddr(STATION_IF, mac);
	if (ret == false)
		os_printf("ERR: Can not get MAC address!\n");
	mac_full = 0;
	for (i=0;i<NETIF_MAX_HWADDR_LEN;i++) {
		mac_full <<= 8;
		mac_full |= mac[i];
	}
	mac_full = 9223372036854775807UL;

	os_printf("DBG: SDK version:%s\n", system_get_sdk_version());
	os_printf("DBG: ESP8266 chip ID:0x%x\n", system_get_chip_id());
	os_printf("DBG: AIR version: " AIR_VERSION " \n");
	os_printf("DBG: MAC address: %lu\n", mac_full);
	os_printf("DBG: integer size int %d long %li long long %lli\n",1,1L,1LL);


	// Initialize spiffs
	user_spiffs();

	// Create queues
	user_create_queues();

	// Start STATIONAP mode for scanning and for connection
//	xTaskCreate(softap_task,"softap_task",500,NULL,6,NULL);
	xTaskCreate(station_task,"station_task",500,NULL,6,NULL);

	// Start TCP server
	espconn_init();

//	user_mdns_config();

	// Start task led
//	xTaskCreate(task_led, "led driver", 256, &led_type, 2, NULL);

	// Main task - state machine
	xTaskCreate(task_main, "main", 1024, NULL, 2, NULL);

	// Start task sds011
//	xTaskCreate(task_sds011, "sds011 driver", 256, NULL, 2, NULL);

}

