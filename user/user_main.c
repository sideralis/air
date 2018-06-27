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
#include "lwip/mdns.h"
#include "lwip/netif.h"

#include "freertos/queue.h"

#ifdef DEBUG
#include "esp8266/ets_sys.h"
#include <../esp-gdbstub/gdbstub.h>
#endif

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

void user_tcpserver_init(uint32 );
void user_set_softap_config();

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
	int nb_ap;								// Nb of ap air can connect to
	struct station_config config[5];		// Information on these ap
	struct led_info led_setup;
	signed portBASE_TYPE ret;
	struct bss_info *wifi_scan;
	xTaskHandle handle_task_wifi;
	struct station_config station_info;

    struct router_info *info = NULL;

	while(1) {
		// Get ap info
		//wifi_station_get_current_ap_id();
		nb_ap = wifi_station_get_ap_info(config);
		printf("DBG: Data on station = %d\n", nb_ap);
		if (nb_ap < 50) {			// FIXME: only for debug, should be == 0
			// We have never connected to any network, let's scan for wifi
			// Start task wifi scan
			xTaskCreate(task_wifi_scan, "Scan Wifi Around", 256, NULL, 2, &handle_task_wifi);

			led_setup.color_to = LED_BLUE;
			led_setup.color_from = LED_BLACK;
			led_setup.state = LED_BLINK;
			xQueueSend(led_queue, &led_setup, 0);

			ret = xQueueReceive(wifi_scan_queue, &router_data, 10000 / portTICK_RATE_MS);			// Timeout after 10s
			if (ret == errQUEUE_EMPTY) {
				printf("DBG: No wifi dectected!\n");
				// No wifi detected
			} else {
				printf("DBG: Wifi dectected!\n");
				// Wifi detected, we move to AP

				// Change LED to indicate we move to next step
				led_setup.color_to = LED_CYAN;
				led_setup.color_from = LED_BLUE;
				led_setup.state = LED_BLINK;
				xQueueSend(led_queue, &led_setup, 0);

				// Display networks
				SLIST_FOREACH(info, &router_data, next) {
					printf("INFO: Network info: %s %d %d\n", info->ssid, info->rssi, info->authmode);
				}

				// Move to AP: already done as we started as both

				// Start TCP server
				os_printf("DBG: Start TCP server\n");
				user_tcpserver_init(SERVER_LOCAL_PORT);

				// Wait for user network selection
				ret = xQueueReceive(network_queue, &station_info, portMAX_DELAY);

				// User has now selected network, let's try to connect to it:
				os_printf("DBG: Trying to connect to: %s with password: %s\n",station_info.ssid, station_info.password);
				convert_UTF8_string(station_info.ssid);
				convert_UTF8_string(station_info.password);
				os_printf("DBG: After UTF8 conversion - to: %s with password: %s\n",station_info.ssid, station_info.password);

				wifi_station_set_config(&station_info);
				wifi_set_event_handler_cb(wifi_handle_event_cb);
				wifi_station_connect();
				os_printf("DBG: We should be connected now\n");
			}
		} else {
			printf("DBG: We have already connected to a network. Trying again...\n");
		}
		vTaskSuspend(xTaskGetCurrentTaskHandle());
	}
}

//int user_softAP_config(u32t local_ip, u32t gateway, u32t subnet) {
//    int ret = true;
//
//    struct ip_info info;
//    info.ip.addr = local_ip;
//    info.gw.addr = gateway;
//    info.netmask.addr = subnet;
//
//    if(!wifi_softap_dhcps_stop()) {
//        printf("[APConfig] wifi_softap_dhcps_stop failed!\n");
//    }
//
//    if(!wifi_set_ip_info(SOFTAP_IF, &info)) {
//        printf("[APConfig] wifi_set_ip_info failed!\n");
//        ret = false;
//    }
//
//    struct dhcps_lease dhcp_lease;
//    u32t ip = local_ip;
//    ip[3] += 99;
//    dhcp_lease.start_ip.addr = static_cast<uint32_t>(ip);
//    printf("[APConfig] DHCP IP start: %s\n", ip);
//
//    ip[3] += 100;
//    dhcp_lease.end_ip.addr = static_cast<uint32_t>(ip);
//    DEBUG_WIFI("[APConfig] DHCP IP end: %s\n", ip.toString().c_str());
//
//    if(!wifi_softap_set_dhcps_lease(&dhcp_lease)) {
//        DEBUG_WIFI("[APConfig] wifi_set_ip_info failed!\n");
//        ret = false;
//    }
//
//    // set lease time to 720min --> 12h
//    if(!wifi_softap_set_dhcps_lease_time(720)) {
//        DEBUG_WIFI("[APConfig] wifi_softap_set_dhcps_lease_time failed!\n");
//        ret = false;
//    }
//
//    uint8 mode = 1;
//    if(!wifi_softap_set_dhcps_offer_option(OFFER_ROUTER, &mode)) {
//        DEBUG_WIFI("[APConfig] wifi_softap_set_dhcps_offer_option failed!\n");
//        ret = false;
//    }
//
//    if(!wifi_softap_dhcps_start()) {
//        DEBUG_WIFI("[APConfig] wifi_softap_dhcps_start failed!\n");
//        ret = false;
//    }
//
//    // check config
//    if(wifi_get_ip_info(SOFTAP_IF, &info)) {
//        if(info.ip.addr == 0x00000000) {
//            DEBUG_WIFI("[APConfig] IP config Invalid?!\n");
//            ret = false;
//        } else if(local_ip != info.ip.addr) {
//            ip = info.ip.addr;
//            DEBUG_WIFI("[APConfig] IP config not set correct?! new IP: %s\n", ip.toString().c_str());
//            ret = false;
//        }
//    } else {
//        DEBUG_WIFI("[APConfig] wifi_get_ip_info failed!\n");
//        ret = false;
//    }
//
//    return ret;
//}



/******************************************************************************
 * FunctionName : user_set_softap_config
 * Description  : set SSID and password of ESP8266 softAP
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void user_set_softap_config(void) {
	int ret;
	struct softap_config *config = (struct softap_config *) zalloc(sizeof(struct softap_config)); // initialization
    struct ip_info ip;
    u8_t mac[NETIF_MAX_HWADDR_LEN];

	// Get config
	ret = wifi_softap_get_config(config);

	if (ret == 0) {
		os_printf("ERR: Could not get softap config!\n");
		return;
	}

    wifi_get_macaddr(SOFTAP_IF, mac);

	// Modify it
	memset(config->ssid, 0, 32);
	memset(config->password, 0, 64);
	memcpy(config->ssid, "AIR-XX", 6);				// TODO: XX should be replaced by hash of mac address
	//memcpy(config->password, "12345678", 8);		// No password as open wifi
	config->authmode = AUTH_OPEN;
	config->ssid_len = 6; 							// or its actual length
	config->max_connection = 4; 					// how many stations can connect to ESP8266 softAP at most (4 is max)

	// Save it
	ETS_UART_INTR_DISABLE();
	ret = wifi_softap_set_config(config); 			// Set ESP8266 softap config
	ETS_UART_INTR_ENABLE();

	if (ret == 0) {
		os_printf("ERR: Could not set softap config!\n");
		return;
	}

	// Check DHCP is started
	if (wifi_softap_dhcps_status() != DHCP_STARTED) {
		os_printf("ERR: DHCP not started, starting...\n");
		if (!wifi_softap_dhcps_start()) {
			os_printf("ERR: wifi_softap_dhcps_start failed!\n");
		}
	}

    // Check IP config
    if (wifi_get_ip_info(SOFTAP_IF, &ip)) {
        if (ip.ip.addr == 0x00000000) {
            // Invalid config
        	os_printf("ERR: IP config Invalid resetting...\n");
            //192.168.244.1 , 192.168.244.1 , 255.255.255.0
//            ret = softAPConfig(0x01F4A8C0, 0x01F4A8C0, 0x00FFFFFF);
//            if(!ret) {
//            	os_printf("ERR: softAPConfig failed!\n");
//                ret = false;
//        	}
        }
    } else {
    	os_printf("ERR: wifi_get_ip_info failed!\n");
    }

	free(config);
}



/**
 * mDNS configuration
 * Not yet working!!!
 */
void user_mdns_config()
{
	struct ip_info ipconfig;
	struct mdns_info *info;

	wifi_get_ip_info(STATION_IF, &ipconfig);

	if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {

		info = (struct mdns_info *)zalloc(sizeof(struct mdns_info));
		if (info == 0)
			return;

		info->host_name = "air";
		info->ipAddr = ipconfig.ip.addr;			// ESP8266 Station IP
		info->server_name = "iot";
		info->server_port = 80;
		info->txt_data[0] = "version = now";
		info->txt_data[1] = "user1 = data1";
		info->txt_data[2] = "user2 = data2";

		espconn_mdns_init(info);
	}
}
/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void user_init(void) {

	// Reconfigure UART to 115200 bauds
	uart_init_new();

	os_printf("SDK version:%s\n", system_get_sdk_version());
	os_printf("ESP8266	chip	ID:0x%x\n", system_get_chip_id());
	os_printf("AIR version: " AIR_VERSION " \n");

#ifdef DEBUG
//	gdbstub_init();
#endif

	// Create queues
	user_create_queues();

	// Start SATIONAP mode for scanning and for connection
   xTaskCreate(softap_task,"softap_task",500,NULL,6,NULL);

	// Start TCP server
	espconn_init();
//	user_tcpserver_init(SERVER_LOCAL_PORT);

//	user_mdns_config();

	// Start task led
	xTaskCreate(task_led, "led driver", 256, NULL, 2, NULL);

	// Main task - state machine
	xTaskCreate(task_main, "main", 1024, NULL, 2, NULL);


	// Start task sds011
	//xTaskCreate(task_sds011, "sds011 driver", 256, NULL, 2, NULL);

}

