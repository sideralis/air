/*
 * user_wifi.c
 *
 *  Created on: 21 juin 2018
 *      Author: gautier
 */

#include "esp_common.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "json/cJSON.h"

#include "spiffs.h"
#include <fcntl.h>

#include "user_queue.h"
#include "user_wifi.h"
#include "user_led.h"

SLIST_HEAD(router_info_head, router_info) router_list;
uint8  current_channel;
uint16 channel_bits;

/**
 *
 */
void wifi_handle_event_cb(System_Event_t *evt)
{
	struct led_info led_setup;
	int got_ip = 0;

	//printf("event %x\n", evt->event_id);

	if (evt == NULL)
		return;

    switch (evt->event_id) {
        case EVENT_STAMODE_CONNECTED:
            printf("connect to ssid %s, channel %d\n", evt->event_info.connected.ssid,
                    evt->event_info.connected.channel);
            break;
        case EVENT_STAMODE_DISCONNECTED:
            printf("disconnect from ssid %s, reason %d\n", evt->event_info.disconnected.ssid,
                    evt->event_info.disconnected.reason);
                    // FIXME: we should handle this case!
            break;
        case EVENT_STAMODE_AUTHMODE_CHANGE:
            printf("mode: %d -> %d\n", evt->event_info.auth_change.old_mode, evt->event_info.auth_change.new_mode);
            break;
        case EVENT_STAMODE_GOT_IP:
            printf("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR, IP2STR(&evt->event_info.got_ip.ip),
                    IP2STR(&evt->event_info.got_ip.mask), IP2STR(&evt->event_info.got_ip.gw));
            got_ip = 1;
            xQueueSend(got_ip_queue, &got_ip, 0);
            printf("\n");
            break;
        case EVENT_SOFTAPMODE_STACONNECTED:
            printf("station: " MACSTR "join, AID = %d\n", MAC2STR(evt->event_info.sta_connected.mac),
                    evt->event_info.sta_connected.aid);

            // Stop blinking to indicate we are connected
			led_setup.color_to = LED_WHITE;
			led_setup.state = LED_ON;
			xQueueSend(led_queue, &led_setup, 0);

            break;
        case EVENT_SOFTAPMODE_STADISCONNECTED:
            printf("station: " MACSTR "leave, AID = %d\n", MAC2STR(evt->event_info.sta_disconnected.mac),
                    evt->event_info.sta_disconnected.aid);

            // Start blinking to indicate we are disconnected
			led_setup.color_to = LED_WHITE;
			led_setup.color_from = LED_BLACK;
			led_setup.state = LED_BLINK;
			xQueueSend(led_queue, &led_setup, 0);

            break;
        default:
//        	printf("event %x\n", evt->event_id);
            break;
    }
}

/**
 * Function which is called when wifi scan is completed
 */
void ICACHE_FLASH_ATTR wifi_scan_done(void *arg, STATUS status)
{
    struct router_info *info = NULL;
	signed portBASE_TYPE ret;
	int status_scan = WIFI_NOT_DETECTED;

    // Empty list
    while ((info = SLIST_FIRST(&router_list)) != NULL) {
        SLIST_REMOVE_HEAD(&router_list, next);
        free(info);
    }

    if (status == OK) {
        uint8 i;
        struct bss_info *bss = (struct bss_info *) arg;

		ret = xQueueReset(wifi_scan_queue);
		if (ret == pdFAIL) {
			// Queue is being read so let's wait for next scan
			os_printf("WAR: Trying to reset queue while being used!\n");
			return;
		}

        // Parse all detected networks
        while (bss != NULL) {
            if (bss->channel != 0) {
                struct router_info *info = NULL;
                os_printf("DBG: ssid %s, channel %d, authmode %d, rssi %d\n", bss->ssid, bss->channel, bss->authmode, bss->rssi);
                info = (struct router_info *) zalloc(sizeof(struct router_info));
                info->authmode = bss->authmode;						// WIFI mode
                info->channel = bss->channel;						// WIFI channel
                info->rssi = bss->rssi;								// WIFI rssi
                memcpy(info->bssid, bss->bssid, 6);					// WIFI mac add
				if (strlen(bss->ssid) <= 32)
					memcpy(info->ssid, bss->ssid, strlen(bss->ssid));
				else
					memcpy(info->ssid, bss->ssid, 32);				// WIFI name

                SLIST_INSERT_HEAD(&router_list, info, next);
            }
            bss = STAILQ_NEXT(bss, next);
        }
        // Queue result
		if (!(SLIST_EMPTY(&router_list))) {
			char *buf;
			cJSON *wifi_list = cJSON_CreateObject();				// FIXME check if wifi_list == NULL
			cJSON *list = cJSON_CreateArray();						// FIXME check if list == NULL
		    cJSON_AddItemToObject(wifi_list, "names", list);
			SLIST_FOREACH(info, &router_list, next) {
				cJSON *wifi = cJSON_CreateString(info->ssid);		// FIXME check if wifi == NULL
		        cJSON_AddItemToArray(list, wifi);
			}
			buf = cJSON_Print(wifi_list);

			int pfd;
			pfd = open("/wifilist.json", O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
			if (pfd <= 3) {
				printf("ERR: open file error!\n");
			}
			int write_byte = write(pfd, buf, strlen(buf));
			if (write_byte <= 0) {
				printf("ERR: write file error (wifilist.json)\n");
			}
			close(pfd);
			free(buf);
			os_printf("DBG: wifilist.json file written.\n");


			ret = xQueueSend(wifi_scan_queue, &router_list, 0); 		// 0 = Don't wait if queue is full
			if (ret != pdPASS) {
				os_printf("WARNING: Problem when queuing scanned wifi!\n");
			}
			status_scan = WIFI_DETECTED;
		}

    } else {
        os_printf("err, scan status %d\n", status);
    }
	ret = xQueueSend(status_scan_queue, &status_scan, 0);
	if (ret != pdPASS) {
		os_printf("WARNING: Problem when queuing status scan!\n");
	}

}

/**
 * Start wifi scan only if we are in STATION mode
 * Fonction is not used for now!
 */
void ICACHE_FLASH_ATTR create_task_wifi_scan(void) {
	if (wifi_get_opmode() == SOFTAP_MODE) {
		os_printf("SoftAP mode is enabled. Enable station mode to scan...\r\n");
		return;
	}
}

/**
 * Task to scan nearby wifi
 */
void task_wifi_scan(void *param) {
	signed portBASE_TYPE ret;
	int scan_status;

	os_printf("DBG: entering wifi scan task\n");

	while(1) {
		os_printf("DBG: Scanning\n");
		wifi_station_scan(NULL, wifi_scan_done);
		ret = xQueueReceive(status_scan_queue, &scan_status, portMAX_DELAY);			// Wait indefinitely
		if (scan_status == WIFI_DETECTED)
			break;																		// TODO: We should exit only if we are sure we have found all networks
	}

	os_printf("DBG: deleting wifi scan task\n");

	vTaskDelete(NULL);
}
void conn_ap_init(void)
{
    wifi_set_opmode(STATION_MODE);
}

void soft_ap_init(void)
{
    wifi_set_opmode_current(SOFTAP_MODE/*STATIONAP_MODE*/);

    struct softap_config *config = (struct softap_config *) zalloc(sizeof(struct softap_config)); // initialization

    wifi_softap_get_config(config); // Get soft-AP config first.

	memset(config->ssid, 0, 32);
	memset(config->password, 0, 64);
	memcpy(config->ssid, "AIR-XX", 6);			// TODO: XX should be replaced by hash of mac address
    config->authmode = AUTH_OPEN;
    config->ssid_len = 6; // or its actual SSID length
    config->max_connection = 4;

    os_printf("DBG: channel used in softap (1/2): %d\n", config->channel);
    wifi_softap_set_config(config); // Set ESP8266 soft-AP config

    free(config);

//    struct station_info * station = wifi_softap_get_station_info();
//    while (station) {
//        printf("bssid : MACSTR, ip : IPSTR/n", MAC2STR(station->bssid), IP2STR(&station->ip));
//        station = STAILQ_NEXT(station, next);
//    }
//    wifi_softap_free_station_info(); // Free it by calling functionss
//    wifi_softap_dhcps_stop(); // disable soft-AP DHCP server
//
//    struct ip_info info;
//    IP4_ADDR(&info.ip, 192, 168, 5, 1); // set IP
//    IP4_ADDR(&info.gw, 192, 168, 5, 1); // set gateway
//    IP4_ADDR(&info.netmask, 255, 255, 255, 0); // set netmask
//    wifi_set_ip_info(SOFTAP_IF, &info);
//    struct dhcps_lease dhcp_lease;
//    IP4_ADDR(&dhcp_lease.start_ip, 192, 168, 5, 100);
//    IP4_ADDR(&dhcp_lease.end_ip, 192, 168, 5, 105);
//    wifi_softap_set_dhcps_lease(&dhcp_lease);
//    wifi_softap_dhcps_start(); // enable soft-AP DHCP server
}

void station_task(void *param) {
    conn_ap_init();
    vTaskDelete(NULL);
}

void softap_task(void *param) {
	soft_ap_init();
	vTaskDelete(NULL);
}

void stationap_task(void *param) {

}
