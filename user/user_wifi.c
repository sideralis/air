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

SLIST_HEAD(router_info_head, router_info)
router_list, router_list_cleaned;	// Cleaned means duplicates removed and ordered by decreasing rssi
uint8 current_channel;
uint16 channel_bits;

struct ip_addr my_ip;

static void wifi_list_remove_duplicates(struct router_info_head *router_list, struct router_info_head *router_list_cleaned)
{
	int found_duplicate;
	struct router_info *info1 = NULL, *info2 = NULL;

	SLIST_FOREACH(info1, router_list, next)
	{
		found_duplicate = false;
		SLIST_FOREACH(info2, router_list_cleaned, next)
		{
			if (strcmp(info1->ssid, info2->ssid) == 0) {
				// This is a found_duplicate, let's replace it if rssi is better.
				found_duplicate = true;
				if (info1->rssi > info2->rssi) {
					info2->rssi = info1->rssi;
					info2->channel = info1->channel;
					if (memcmp(info1->bssid, info2->bssid, 6) == 0)
						os_printf("ERR: bssid should have been different!\n");
					if (info1->authmode != info2->authmode)
						os_printf("ERR: authmode should have matched!\n");
				}
			}
		}
		if (found_duplicate == false) {
			// this is a new network, let's add it
			info2 = (struct router_info *) zalloc(sizeof(struct router_info));
			info2->authmode = info1->authmode;						// WIFI mode
			info2->channel = info1->channel;							// WIFI channel
			info2->rssi = info1->rssi;								// WIFI rssi
			memcpy(info2->bssid, info1->bssid, 6);					// WIFI mac add
			if (strlen(info1->ssid) <= 32)
				memcpy(info2->ssid, info1->ssid, strlen(info1->ssid));
			else
				memcpy(info2->ssid, info1->ssid, 32);				// WIFI name

			SLIST_INSERT_HEAD(router_list_cleaned, info2, next);
		}
	}
}

static void wifi_list_order(struct router_info_head *router_list, struct router_info_head *router_list_ordered)
{
	int inserted;
	struct router_info *info1 = NULL, *info2 = NULL, *info3 = NULL, *info_insert = NULL;

	SLIST_FOREACH(info1, router_list, next)
	{
		inserted = false;
		SLIST_FOREACH(info2, router_list_ordered, next)
		{
			if (info2->rssi > info1->rssi) {
				info_insert = info2;
				inserted = true;
			} else
				break;
		}
		info3 = (struct router_info *) zalloc(sizeof(struct router_info));
		info3->authmode = info1->authmode;						// WIFI mode
		info3->channel = info1->channel;							// WIFI channel
		info3->rssi = info1->rssi;								// WIFI rssi
		memcpy(info3->bssid, info1->bssid, 6);					// WIFI mac add
		if (strlen(info1->ssid) <= 32)
			memcpy(info3->ssid, info1->ssid, strlen(info1->ssid));
		else
			memcpy(info3->ssid, info1->ssid, 32);				// WIFI name

		if (inserted == false) {
			SLIST_INSERT_HEAD(router_list_ordered, info3, next);
		} else {
			SLIST_INSERT_AFTER(info_insert, info3, next);
		}
	}
}

static void wifi_list_clear(struct router_info_head *router_list)
{
	struct router_info *info = NULL;
	while ((info = SLIST_FIRST(router_list)) != NULL) {
		SLIST_REMOVE_HEAD(router_list, next);
		free(info);
	}
}

// -------------------------------------------------------------------------------------------------
/**
 *
 */
void wifi_handle_event_cb(System_Event_t *evt)
{
	struct led_info led_setup;
	int got_ip = 0;

	//os_printf("event %x\n", evt->event_id);

	if (evt == NULL)
		return;

	switch (evt->event_id) {
	case EVENT_STAMODE_CONNECTED:
		os_printf("INFO: connect to ssid %s, channel %d\n", evt->event_info.connected.ssid, evt->event_info.connected.channel);
		break;
	case EVENT_STAMODE_DISCONNECTED:
		os_printf("INFO: disconnect from ssid %s, reason %d\n", evt->event_info.disconnected.ssid, evt->event_info.disconnected.reason);
		// TODO: we should handle this case!
		break;
	case EVENT_STAMODE_AUTHMODE_CHANGE:
		os_printf("INFO: mode: %d -> %d\n", evt->event_info.auth_change.old_mode, evt->event_info.auth_change.new_mode);
		break;
	case EVENT_STAMODE_GOT_IP:
		os_printf("INFO: ip: " IPSTR ", mask: " IPSTR ", gw: " IPSTR, IP2STR(&evt->event_info.got_ip.ip), IP2STR(&evt->event_info.got_ip.mask),
				IP2STR(&evt->event_info.got_ip.gw));
		os_printf("\n");
		memcpy(&my_ip, &evt->event_info.got_ip.ip, sizeof(my_ip));
		got_ip = 1;
		xQueueSend(got_ip_queue, &got_ip, 0);
		break;
	case EVENT_SOFTAPMODE_STACONNECTED:
		os_printf("INFO: station: " MACSTR "join, AID = %d\n", MAC2STR(evt->event_info.sta_connected.mac), evt->event_info.sta_connected.aid);

		// Stop blinking to indicate we are connected
		led_setup.color_to = LED_WHITE;
		led_setup.state = LED_ON;
		xQueueSend(led_queue, &led_setup, 0);

		break;
	case EVENT_SOFTAPMODE_STADISCONNECTED:
		os_printf("INFO: station: " MACSTR "leave, AID = %d\n", MAC2STR(evt->event_info.sta_disconnected.mac), evt->event_info.sta_disconnected.aid);

		// Start blinking to indicate we are disconnected
		led_setup.color_to = LED_WHITE;
		led_setup.color_from = LED_BLACK;
		led_setup.state = LED_BLINK;
		xQueueSend(led_queue, &led_setup, 0);

		break;
	default:
//        	os_printf("event %x\n", evt->event_id);
		break;
	}
}

/**
 * Function which is called when wifi scan is completed
 */
void wifi_scan_done(void *arg, STATUS status)
{
	struct router_info *info = NULL, *info2 = NULL;
	signed portBASE_TYPE ret;
	int status_scan = WIFI_NOT_DETECTED;

	// Empty list
	wifi_list_clear(&router_list);

	if (status == OK) {
		uint8 i;
		struct bss_info *bss = (struct bss_info *) arg;

		ret = xQueueReset(wifi_scan_queue);
		if (ret == pdFAIL) {
			// Queue is being read so let's wait for next scan
			os_printf("WARNING: Trying to reset queue while being used!\n");
			return;
		}

		// Parse all detected networks
		while (bss != NULL) {
			if (bss->channel != 0) {
				struct router_info *info = NULL;
//                os_printf("DBG: ssid %s, channel %d, authmode %d, rssi %d bssid %0x%0x%0x%0x%0x%0x\n", bss->ssid, bss->channel, bss->authmode, bss->rssi,
//                		bss->bssid[0], bss->bssid[1], bss->bssid[2], bss->bssid[3], bss->bssid[4], bss->bssid[5]);
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
		// Remove duplicated networks and, in a second step, order them, from router_list to router_list_cleaned then back to router_list
//		os_printf("\n");
//		SLIST_FOREACH(info, &router_list, next) {
//			os_printf("DBG1: %s %i\n", info->ssid, info->channel);
//		}

//		wifi_list_remove_duplicates(&router_list, &router_list_cleaned);

		//		os_printf("\n");
//		SLIST_FOREACH(info, &router_list_cleaned, next) {
//			os_printf("DBG2: %s %i\n", info->ssid, info->channel);
//		}

//		wifi_list_clear(&router_list);
//		wifi_list_order(&router_list_cleaned, &router_list);

//		os_printf("\n");
//		SLIST_FOREACH(info, &router_list, next) {
//			os_printf("DBG3: %s %i %d %d %0x%0x%0x%0x%0x%0x\n", info->ssid, info->channel, info->authmode, info->rssi,
//					info->bssid[0], info->bssid[1], info->bssid[2], info->bssid[3], info->bssid[4], info->bssid[5]);
//		}

//		wifi_list_clear(&router_list_cleaned);

		// Queue result
		if (!(SLIST_EMPTY(&router_list))) {
			char *buf;
			cJSON *wifi_list = cJSON_CreateObject();				// FIXME check if wifi_list == NULL
			cJSON *list = cJSON_CreateArray();						// FIXME check if list == NULL
			cJSON_AddItemToObject(wifi_list, "names", list);
			SLIST_FOREACH(info, &router_list, next)
			{
				cJSON *wifi = cJSON_CreateString(info->ssid);		// FIXME check if wifi == NULL
				cJSON_AddItemToArray(list, wifi);
			}
			buf = cJSON_Print(wifi_list);

			int pfd;
			pfd = open("/wifilist.json", O_TRUNC | O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
			if (pfd <= 3) {
				os_printf("ERR: open file error!\n");
			}
			int write_byte = write(pfd, buf, strlen(buf));
			if (write_byte <= 0) {
				os_printf("ERR: write file error (wifilist.json)\n");
			}
			close(pfd);
			free(buf);
//			os_printf("DBG: wifilist.json file written.\n");

			ret = xQueueSend(wifi_scan_queue, &router_list, 0); 		// 0 = Don't wait if queue is full
			if (ret != pdPASS) {
				os_printf("WARNING: Problem when queuing scanned wifi!\n");
			}
			status_scan = WIFI_DETECTED;
		}

	} else {
		os_printf("ERR: scan status %d\n", status);
	}
	ret = xQueueSend(status_scan_queue, &status_scan, 0);
	if (ret != pdPASS) {
		os_printf("WARNING: Problem when queuing status scan!\n");
	}

}

/**
 * Task to scan nearby wifi
 */
void task_wifi_scan(void *param)
{
	signed portBASE_TYPE ret;
	int scan_status;

	if (wifi_get_opmode() == SOFTAP_MODE) {
		os_printf("ERR: SoftAP mode is enabled. Enable station mode to scan...\r\n");
		return;
	}

	while (1) {
//		os_printf("DBG: Scanning\n");
		wifi_station_scan(NULL, wifi_scan_done);
		ret = xQueueReceive(status_scan_queue, &scan_status, portMAX_DELAY);			// Wait indefinitely
		if (scan_status == WIFI_DETECTED)
			break;																		// TODO: We should exit only if we are sure we have found all networks
	}

//	os_printf("DBG: deleting wifi scan task\n");

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

//    os_printf("DBG: channel used in softap (1/2): %d\n", config->channel);
	wifi_softap_set_config(config); // Set ESP8266 soft-AP config

	free(config);
}

void station_task(void *param)
{
	conn_ap_init();
	vTaskDelete(NULL);
}

void softap_task(void *param)
{
	soft_ap_init();
	vTaskDelete(NULL);
}

void stationap_task(void *param)
{

}

// ----------------------------------------------------------------------------------------------

