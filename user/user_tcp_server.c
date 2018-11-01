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
#include "user_pages.h"

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

	os_printf("INFO: tcp sent callback\n");
}

// This structure link a page name and a protocol (GET/POST) to a function handler
// FIXME to be moved.
struct page_handler {
	char page_name[32];
	int method;
	int (*handler)(struct header_html_recv *, struct espconn *);
};

// All html pages we can handle
struct page_handler html_page_handler[] = { { "/wifi.html", METHOD_ALL, page_wifi }, { "/connect.html", METHOD_POST, page_connect }, };

/******************************************************************************
 * FunctionName : tcp_server_recv_cb
 * Description  : receive callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
static void tcp_server_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
	char *m;
	int i, err;
	struct espconn *pesp_conn = arg;

	struct header_html_recv *request;

	request = malloc(sizeof(struct header_html_recv));
	if (request == 0) {
		os_printf("ERR: malloc %d %f\n", __LINE__, __FILE__);
		return;
	}

	//received some data from tcp connection

	os_printf("INFO: tcp data receive %c%c%c%c%c%c%c%c...\n", pusrdata[0], pusrdata[1], pusrdata[2], pusrdata[3], pusrdata[4], pusrdata[5], pusrdata[6],
			pusrdata[7]);

	// Extract from web page if this is a get or a push and which page is requested and the parameters of the page
	if (process_header_recv(pusrdata, request)) {
		os_printf("ERR: extraction %d %f\n", __LINE__, __FILE__);
		free(request);
		return;
	}

	/*
	 os_printf("DBG: Request is:\n");
	 os_printf("DBG:   page name = %s\n",request.page_name);
	 os_printf("DBG:   method = %d\n",request.method);
	 os_printf("DBG:   key,value = %s, %s\n",request.form[0].key, request.form[0].value);
	 */

	// Search function to call according to page_name and call the function which match.
	for (i = 0; i < sizeof(html_page_handler) / sizeof(struct page_handler); i++) {
		if ((request->method & html_page_handler[i].method) && (strcmp(request->page_name, html_page_handler[i].page_name) == 0)) {
			err = html_page_handler[i].handler(request, pesp_conn);
			if (err)
				html_render_template("404.html", pesp_conn);
			free(request);
			return;
		}
	}

	// Search for a file matching the page name
	err = html_render_template(request->page_name, pesp_conn);

	// We have found no matching page, so we should send 404.
	if (err)
		html_render_template("404.html", pesp_conn);

	free(request);
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

	os_printf("INFO: tcp disconnect succeed\n");
	if (start_tcpclient == true) {
		os_printf("INFO: reconnect as tcp client\n");
		user_tcpclient_init();
	}
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

	os_printf("INFO: reconnect callback, error code %d\n", err);
}

static void IRAM_ATTR tcp_server_multi_send(void)
{
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

			os_printf("DBG(%d): S=%d P=%d IP1=%d IP2=%d IP3=%d IP4=%d\n", count, premot[count].state, premot[count].remote_port, premot[count].remote_ip[0],
					premot[count].remote_ip[1], premot[count].remote_ip[2], premot[count].remote_ip[3]);

		}
	}
}

void tcpserver_disconnect_and_tcpclient_connect()
{
	struct espconn *pesp_conn = &esp_conn;

	espconn_disconnect(pesp_conn);
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
	os_printf("INFO: tcp connect succeed\n");

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

	if (ret)
		os_printf("ERR: espconn_accept returns %d\n", ret);

}

