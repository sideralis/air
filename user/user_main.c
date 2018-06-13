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
#include "espconn.h"
#include "espressif/esp8266/ets_sys.h"

#include "lwip/sockets.h"
//#include "lwip/mdns.h"
#include "freertos/queue.h"

#include <../esp-gdbstub/gdbstub.h>

#include "gpio.h"

/* Defines */
#define AIR_VERSION	"0.0.1"

/* Prototypes */
char *create_page(void);
void task_sds011(void *);

/* Global variables */
xQueueHandle wifi_scan_queue;				/* Queue used to store wifi scan results */

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

/******************************************************************************
 * FunctionName : user_set_softap_config
 * Description  : set SSID and password of ESP8266 softAP
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void ICACHE_FLASH_ATTR user_set_softap_config(void) {
	int ret;
	struct softap_config *config = (struct softap_config *) zalloc(
			sizeof(struct softap_config)); // initialization
    struct ip_info ip;

	// Get config
	ret = wifi_softap_get_config(config);

	if (ret == 0) {
		os_printf("ERR: Could not get softap config!\n");
		return;
	}

	// Modify it
	memset(config->ssid, 0, 32);
	memset(config->password, 0, 64);
	memcpy(config->ssid, "ESP8266", 7);
	//memcpy(config->password, "12345678", 8);	// No password as open wifi
	config->authmode = AUTH_OPEN;
	config->ssid_len = 7; // or its actual length
	config->max_connection = 4; // how many stations can connect to ESP8266 softAP at most (4 is max)

	// Save it
	ETS_UART_INTR_DISABLE();
	ret = wifi_softap_set_config(config); // Set ESP8266 softap config
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
 * Function which is called when wifi scan is completed
 */
void scan_done(void *arg, STATUS status) {
	uint8 ssid[33];
	char temp[128];
	struct bss_info *wifi_scan;
	signed portBASE_TYPE ret;

	// DBG
	unsigned long nbTask;
	void *h;
	nbTask = uxTaskGetNumberOfTasks();
	os_printf("DBG_scan: Nb of tasks = %d\n",nbTask);
	h = xTaskGetCurrentTaskHandle();
	os_printf("DBG_scan: handle = %p\n",h);
	// END DBG

	if (status == OK) {
		struct bss_info *bss_link = (struct bss_info *) arg;
		ret = xQueueReset(wifi_scan_queue);
		if (ret == pdFAIL) {
			// Queue is being read so let's wait for next scan
			os_printf("WAR: Trying to reset queue while being used!\n");
			return;
		}
		while (bss_link != NULL) {
			// Store data received in a table
			wifi_scan = malloc(sizeof(struct bss_info));
			if (wifi_scan == 0) {
				os_printf("ERR: can not allocate memory to store scanned wifi!\n");
				return;
			}
			memset(wifi_scan,0,sizeof(struct bss_info));
			// - get name
			if (strlen(bss_link->ssid) <= 32)
				memcpy(wifi_scan->ssid, bss_link->ssid, strlen(bss_link->ssid));
			else
				memcpy(wifi_scan->ssid, bss_link->ssid, 32);
			// - get mode
			wifi_scan->authmode = bss_link->authmode;
			// - get rssi
			wifi_scan->rssi = bss_link->rssi;
			// - Set next
			wifi_scan->next.stqe_next = NULL;

			ret = xQueueSend(wifi_scan_queue, &wifi_scan, 0); // 0 = Don't wait if queue is full
			if (ret != pdPASS) {
				os_printf("WARNING: Problem when queuing scanned wifi!\n");
			}

			printf("(%d,\"%s\",%d,\""MACSTR"\",%d)\r\n",
					bss_link->authmode, wifi_scan->ssid, bss_link->rssi,
					MAC2STR(bss_link->bssid), bss_link->channel);
			bss_link = bss_link->next.stqe_next;
		}
	} else {
		printf("scan fail !!!\r\n");
	}
}

// Task to scan nearby wifi
void task_wifi_scan(void *param) {
	unsigned long nbTask;
	void *h;

	os_printf("Welcome to task wifi scan!\n");
	for (;;) {
		wifi_station_scan(NULL, scan_done);
		os_printf("Waiting 10s before new scan ...\n");

		// DBG
		nbTask = uxTaskGetNumberOfTasks();
		os_printf("DBG_scan_task: Nb of tasks = %d\n",nbTask);
		h = xTaskGetCurrentTaskHandle();
		os_printf("DBG_scan_task: handle = %p\n",h);
		// END DBG

		vTaskDelay(10000 / portTICK_RATE_MS);		// Wait 10s
	}
	vTaskDelete(NULL);
}

// ------------------------------
// --------- TCP SERVER ---------
// ------------------------------


LOCAL struct espconn esp_conn;
LOCAL esp_tcp esptcp;

#define SERVER_LOCAL_PORT   80

/******************************************************************************
 * FunctionName : tcp_server_sent_cb
 * Description  : data sent callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_sent_cb(void *arg) {
	//data sent successfully

	os_printf("tcp sent cb \r\n");
}

/******************************************************************************
 * FunctionName : tcp_server_recv_cb
 * Description  : receive callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_recv_cb(void *arg, char *pusrdata, unsigned short length) {
	//received some data from tcp connection

	struct espconn *pespconn = arg;
	os_printf("tcp recv : %s \r\n", pusrdata);

	espconn_sent(pespconn, pusrdata, length);
}

/******************************************************************************
 * FunctionName : tcp_server_discon_cb
 * Description  : disconnect callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_discon_cb(void *arg) {
	//tcp disconnect successfully

	os_printf("tcp disconnect succeed !!! \r\n");
}

/******************************************************************************
 * FunctionName : tcp_server_recon_cb
 * Description  : reconnect callback, error occured in TCP connection.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_recon_cb(void *arg, sint8 err) {
	//error occured , tcp connection broke.

	os_printf("reconnect callback, error code %d !!! \r\n", err);
}

LOCAL void tcp_server_multi_send(void) {
	struct espconn *pesp_conn = &esp_conn;
	char *m;

	remot_info *premot = NULL;
	uint8 count = 0;
	sint8 value = ESPCONN_OK;
	if (espconn_get_connection_info(pesp_conn, &premot, 0) == ESPCONN_OK) {
		char *pbuf = "tcp_server_multi_send\n";
		for (count = 0; count < pesp_conn->link_cnt; count++) {
			pesp_conn->proto.tcp->remote_port = premot[count].remote_port;

			pesp_conn->proto.tcp->remote_ip[0] = premot[count].remote_ip[0];
			pesp_conn->proto.tcp->remote_ip[1] = premot[count].remote_ip[1];
			pesp_conn->proto.tcp->remote_ip[2] = premot[count].remote_ip[2];
			pesp_conn->proto.tcp->remote_ip[3] = premot[count].remote_ip[3];

			os_printf("DEBUG(%d): S=%d P=%d IP1=%d IP2=%d IP3=%d IP4=%d\n",
					count, premot[count].state, premot[count].remote_port,
					premot[count].remote_ip[0], premot[count].remote_ip[1],
					premot[count].remote_ip[2], premot[count].remote_ip[3]);

			m = create_page();

			os_printf("DATA = %s\n", m);

			espconn_send(pesp_conn, m, strlen(m));

			free(m);
		}
	}
}

/******************************************************************************
 * FunctionName : tcp_server_listen
 * Description  : TCP server listened a connection successfully
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_listen(void *arg) {
	struct espconn *pesp_conn = arg;
	os_printf("tcp_server_listen !!! \r\n");

	espconn_regist_recvcb(pesp_conn, tcp_server_recv_cb);
	espconn_regist_reconcb(pesp_conn, tcp_server_recon_cb);
	espconn_regist_disconcb(pesp_conn, tcp_server_discon_cb);

	espconn_regist_sentcb(pesp_conn, tcp_server_sent_cb);
	tcp_server_multi_send();
}

/******************************************************************************
 * FunctionName : user_tcpserver_init
 * Description  : parameter initialize as a TCP server
 * Parameters   : port -- server port
 * Returns      : none
 *******************************************************************************/
void ICACHE_FLASH_ATTR
user_tcpserver_init(uint32 port) {
	esp_conn.type = ESPCONN_TCP;
	esp_conn.state = ESPCONN_NONE;
	esp_conn.proto.tcp = &esptcp;
	esp_conn.proto.tcp->local_port = port;
	espconn_regist_connectcb(&esp_conn, tcp_server_listen);

	sint8 ret = espconn_accept(&esp_conn);

	os_printf("espconn_accept [%d] !!! \r\n", ret);

}
void ICACHE_FLASH_ATTR create_task_wifi_scan(void) {
	if (wifi_get_opmode() == SOFTAP_MODE) {
		os_printf("SoftAP mode is enabled. Enable station mode to scan...\r\n");
		return;
	}
	xTaskCreate(task_wifi_scan, "Scan Wifi Around", 256, NULL, 2, NULL);
}



//void IRAM_ATTR user_mdns_config()
//{
//	struct ip_info ipconfig;
//	struct mdns_info *info;
//
//
////	wifi_get_ip_info(STATION_IF, &ipconfig);
//
//	info = (struct mdns_info *)zalloc(sizeof(struct mdns_info));
//	if (info == 0)
//		return;
//
//	info->host_name = "air";
//	info->ipAddr = ipconfig.ip.addr;			// ESP8266 Station IP
//	info->server_name = "iot";
//	info->server_port = 8080;
//	info->txt_data[0] = "version = now";
//	info->txt_data[1] = "user1 = data1";
//	info->txt_data[2] = "user2 = data2";
//
////	espconn_mdns_init(info);
//}

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
	gdbstub_init();
#endif

//	user_mdns_config();

	// Create queues
	wifi_scan_queue = xQueueCreate( 10, sizeof(struct bss_info *) );

	// Set softAP + station mode
	wifi_set_opmode_current(/*SOFTAP_MODE*/STATIONAP_MODE);

	// Wait for end of init before scanning nearby wifi
	if (wifi_get_opmode() == SOFTAP_MODE) {
		os_printf("SoftAP mode is enabled. Enable station mode to scan...\r\n");
		return;
	}

	// ESP8266 softAP set config.
	user_set_softap_config();

	// Start TCP server
	espconn_init();
	user_tcpserver_init(SERVER_LOCAL_PORT);


//	user_mdns_config();

	// Start task wifi scan
//	xTaskCreate(task_wifi_scan, "Scan Wifi Around", 256, NULL, 2, NULL);
	create_task_wifi_scan();

	// Start task sds011
//	xTaskCreate(task_sds011, "sds011 driver", 256, NULL, 2, NULL);

}

