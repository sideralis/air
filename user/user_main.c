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

#include "espconn.h"

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
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
void ICACHE_FLASH_ATTR
user_set_softap_config(void) {
	struct softap_config *config = (struct softap_config *) zalloc(
			sizeof(struct softap_config)); // initialization

	wifi_softap_get_config(config); // Get config first.

	memset(config->ssid, 0, 32);
	memset(config->password, 0, 64);
	memcpy(config->ssid, "ESP8266", 7);
	memcpy(config->password, "12345678", 8);
	config->authmode = AUTH_WPA_WPA2_PSK;
	config->ssid_len = 0; // or its actual length
	config->max_connection = 4; // how many stations can connect to ESP8266 softAP at most.

	wifi_softap_set_config(config); // Set ESP8266 softap config

	free(config);

}
#define	HTTPD_SERVER_PORT	1002
#define MAX_CONN			32

void tcp_server(void *param) {
	int32 listenfd;
	int32 ret;
	struct sockaddr_in server_addr, remote_addr;
	int stack_counter = 0;

	/* Construct local address structure */
	memset(&server_addr, 0, sizeof(server_addr)); /* Zero out structure */
	server_addr.sin_family = AF_INET; /* Internet address family */
	server_addr.sin_addr.s_addr = INADDR_ANY; /* Any incoming interface */
	server_addr.sin_len = sizeof(server_addr);
	server_addr.sin_port = htons(HTTPD_SERVER_PORT); /* Local port */

	/* Create socket for incoming connections */
	do {
		listenfd = socket(AF_INET, SOCK_STREAM, 0);
		if (listenfd == -1) {
			printf("ESP8266 TCP server ask > socket error\n");
			vTaskDelay(1000 / portTICK_RATE_MS);
		}
	} while (listenfd == -1);

	printf("ESP8266 TCP server task > create socket: %d\n", listenfd);

	/* Bind to the local port */
	do {
		ret = bind(listenfd, (struct sockaddr * ) &server_addr,
				sizeof(server_addr));
		if (ret != 0) {
			printf("ESP8266 TCP server task > bind fail\n");
			vTaskDelay(1000 / portTICK_RATE_MS);
		}
	} while (ret != 0);
	printf("ESP8266 TCP server task > port: %d\n", ntohs(server_addr.sin_port));
	do {
		/*	Listen	to	the	local	connection	*/
		ret = listen(listenfd, MAX_CONN);
		if (ret != 0) {
			printf("ESP8266 TCP server task > failed to set listen queue!\n");
			vTaskDelay(1000 / portTICK_RATE_MS);
		}

	} while (ret != 0);

	printf("ESP8266 TCP server task > listen ok\n");

	int32 client_sock;
	int32 len = sizeof(struct sockaddr_in);
	int recbytes;

	for (;;) {

		printf("ESP8266 TCP server task > wait client\n");

		/*block here waiting remote connect request*/
		if ((client_sock = accept(listenfd, (struct sockaddr * ) &remote_addr,
				(socklen_t * ) &len)) < 0) {

			printf("ESP8266	TCP	server	task	>	accept	fail\n");
			continue;
		}

		printf("ESP8266	TCP	server	task	>	Client	from	%s	%d\n",
				inet_ntoa(remote_addr.sin_addr), htons(remote_addr.sin_port));

		char *recv_buf = (char *) zalloc(128);

		while ((recbytes = read(client_sock, recv_buf, 128)) > 0) {
			recv_buf[recbytes] = 0;
			printf(
					"ESP8266 TCP server task > read data success %d!\nESP8266 TCP server task > %s\n",
					recbytes, recv_buf);
		}

		free(recv_buf);

		if (recbytes <= 0) {
			printf("ESP8266 TCP server task > read data fail!\n");
			close(client_sock);
		}
	}
}

void task1(void *param) {
	os_printf("Welcome to task 1!\n");
	int j, i = 0;
	for (;;) {
		os_printf("-- %d\n", i++);
		for (j = 0; j < 25; j++)
			os_delay_us(40000);
	}
	vTaskDelete(NULL);
}
// ------------------------------
// --------- TCP SERVER ---------
// ------------------------------

static const char html_fmt[128] ICACHE_RODATA_ATTR = "HTTP/1.1 200 OK\r\n"
		"Content-length: %d\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"%s";

static const char html_data[64] ICACHE_RODATA_ATTR
		= "<html><body><h1>Hello Bernard</h1></body></html>";

LOCAL struct espconn esp_conn;
LOCAL esp_tcp esptcp;

#define SERVER_LOCAL_PORT   1112

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

			char *fmt, *data, *m;
			int datasize = sizeof(html_data);
			int fmtsize = sizeof(html_fmt);

			m = (char *) malloc(1024);
			fmt = (char *) malloc(fmtsize);
			data = (char *) malloc(datasize);
			if (m == 0 || fmt == 0 || data == 0) {
				os_printf("Cannot allocate memory to send html answer!\n");
				return;
			}
			system_get_string_from_flash(html_data, data, datasize);
			system_get_string_from_flash(html_fmt, fmt, fmtsize);

			sprintf(m, fmt, strlen(data), data);

			os_printf("DATA = %s\n", m);

			espconn_send(pesp_conn, m, strlen(m));

			free(m);
			free(fmt);
			free(data);
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
/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void user_init(void) {

	// Reconfigure UART to 115200 bauds
	UART_ConfigTypeDef uart_config;
	uart_config.baud_rate = BIT_RATE_115200;
	uart_config.data_bits = UART_WordLength_8b;
	uart_config.parity = USART_Parity_None;
	uart_config.stop_bits = USART_StopBits_1;
	uart_config.flow_ctrl = USART_HardwareFlowControl_None;
	uart_config.UART_RxFlowThresh = 120;
	uart_config.UART_InverseMask = UART_None_Inverse;
	UART_ParamConfig(UART0, &uart_config);

	os_printf("SDK version:%s\n", system_get_sdk_version());
	os_printf("ESP8266	chip	ID:0x%x\n", system_get_chip_id());
	os_printf("AIR\n");

	//Set softAP + station mode
	wifi_set_opmode_current(SOFTAP_MODE);

	// ESP8266 softAP set config.
	user_set_softap_config();

	// Start TCP server
	espconn_init();
	user_tcpserver_init(SERVER_LOCAL_PORT);

//   xTaskCreate(task1, "Task1", 256, NULL, 2, NULL);
//   xTaskCreate(tcp_server, "TCPServer", 256, NULL, 2, NULL);
}

