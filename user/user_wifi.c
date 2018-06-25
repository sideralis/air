/*
 * user_wifi.c
 *
 *  Created on: 21 juin 2018
 *      Author: gautier
 */

#include "esp_common.h"
#include "freertos/queue.h"

#include "user_queue.h"
#include "user_wifi.h"

SLIST_HEAD(router_info_head, router_info) router_list;
uint8  current_channel;
uint16 channel_bits;

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
			break;
	}

	os_printf("DBG: Waiting 10s before terminating wifi scan task\n");
	vTaskDelay(10000 / portTICK_RATE_MS);		// Wait 10s

	vTaskDelete(NULL);
}
