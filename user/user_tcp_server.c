/*
 * user_tcp_server.c
 *
 *  Created on: 21 juin 2018
 *      Author: gautier
 */

#include "esp_common.h"
#include "espconn.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "spiffs.h"
#include <fcntl.h>

#include "user_queue.h"
#include "user_html.h"

static struct espconn esp_conn;
static esp_tcp esptcp;


int html_page_state;

/** Prototypes */

/******************************************************************************
 * FunctionName : tcp_server_sent_cb
 * Description  : data sent callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
static void ICACHE_FLASH_ATTR tcp_server_sent_cb(void *arg)
{
	//data sent successfully

	os_printf("tcp sent cb \r\n");
}

/******************************************************************************
 * FunctionName : tcp_server_recv_cb
 * Description  : receive callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
static void IRAM_ATTR tcp_server_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
	struct header_html_recv html_content;
	char *m;
	//received some data from tcp connection

	struct espconn *pesp_conn = arg;

	os_printf("tcp recv : %s \r\n", pusrdata);

	// Extract from web page if this is a get or a push and which page is requested
	if (process_header_recv(pusrdata, &html_content)) {
		os_printf("ERR: Error in extraction!\n");
		return;
	}


	// Search spiffs for webpage
	int pfd;
	xSemaphoreTake( connect_sem, portMAX_DELAY );
	os_printf("DBG: Start1 opening %s\n", html_content.page);
	pfd = open(html_content.page, O_RDWR);
	os_printf("DBG: End1 opening\n");
	xSemaphoreGive( connect_sem );

	while ((strstr(html_content.page,"json") != 0) && (pfd < 3)) {
		vTaskDelay(1000 / portTICK_RATE_MS);		// Wait 0.5s
		xSemaphoreTake( connect_sem, portMAX_DELAY );
		os_printf("DBG: Start2 opening %s\n", html_content.page);
		pfd = open(html_content.page, O_RDWR);
		xSemaphoreGive( connect_sem );
		os_printf("DBG: End2 opening\n");
	}

	if (pfd < 3) {
		os_printf("ERR: Can't open HTML file! %s\n", html_content.page);
		// TODO return 404
	} else {
		if (html_content.post) {
			os_printf("DBG: POST request\n");
			if (strcmp(html_content.page,"/connect.html") == 0) {
				process_network_choice(pusrdata);
			}
		}

		// Read file
		char *page;
		page = calloc(20000, 1);
		if (page == 0)
			os_printf("ERR: cannot allocate memory for page!\n");

		if (read(pfd, page, 20000) < 0)
			os_printf("ERR: Can't read file %s!\n",html_content.page);
		close(pfd);

		// Add header
		m = html_add_header(page);
		free(page);

//		os_printf("=================\n"
//				"%s\n"
//				"=================\n", m);

		// Return data
		espconn_send(pesp_conn, m, strlen(m));
		free(m);
	}


//	if (strcmp(html_content.page,"/wifi.html") == 0) {
//		if (html_content.get) {
//			os_printf("DBG: A GET request\n");
//			// We should display a status connected or not connected or error connection
//			// We should then display a selection of network status to connect on
//			m = display_network_choice();
//			espconn_send(pesp_conn, m, strlen(m));
//			free(m);
//		} else {
//			os_printf("DBG: A POST request\n");
//			// Here we get the data back, so let's find the network and the password.
//			process_network_choice(pusrdata);
//			m = display_network_choosen();
//			espconn_send(pesp_conn, m, strlen(m));
//			free(m);
//		}
//	} else {
//		display_404();
//	}

}

/******************************************************************************
 * FunctionName : tcp_server_discon_cb
 * Description  : disconnect callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
static void ICACHE_FLASH_ATTR tcp_server_discon_cb(void *arg)
{
	//tcp disconnect successfully

	os_printf("tcp disconnect succeed !!! \r\n");
}

/******************************************************************************
 * FunctionName : tcp_server_recon_cb
 * Description  : reconnect callback, error occured in TCP connection.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
static void ICACHE_FLASH_ATTR tcp_server_recon_cb(void *arg, sint8 err)
{
	//error occured , tcp connection broke.

	os_printf("reconnect callback, error code %d !!! \r\n", err);
}

static void tcp_server_multi_send(void)
{
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

			os_printf("DEBUG(%d): S=%d P=%d IP1=%d IP2=%d IP3=%d IP4=%d\n", count, premot[count].state, premot[count].remote_port, premot[count].remote_ip[0],
					premot[count].remote_ip[1], premot[count].remote_ip[2], premot[count].remote_ip[3]);

//			m = display_network_choice();
//
//			os_printf("DATA = %s\n", m);
//
//			espconn_send(pesp_conn, m, strlen(m));
//
//			free(m);
		}
	}
}

/******************************************************************************
 * FunctionName : tcp_server_listen
 * Description  : TCP server listened a connection successfully
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
static void ICACHE_FLASH_ATTR tcp_server_listen(void *arg)
{
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
void ICACHE_FLASH_ATTR user_tcpserver_init(uint32 port)
{
	esp_conn.type = ESPCONN_TCP;
	esp_conn.state = ESPCONN_NONE;
	esp_conn.proto.tcp = &esptcp;
	esp_conn.proto.tcp->local_port = port;
	espconn_regist_connectcb(&esp_conn, tcp_server_listen);

	sint8 ret = espconn_accept(&esp_conn);

	os_printf("espconn_accept [%d] !!! \r\n", ret);

}

