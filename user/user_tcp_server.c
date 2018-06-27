/*
 * user_tcp_server.c
 *
 *  Created on: 21 juin 2018
 *      Author: gautier
 */

#include "esp_common.h"
#include "espconn.h"
#include "freertos/queue.h"

#include "user_queue.h"

LOCAL struct espconn esp_conn;
LOCAL esp_tcp esptcp;

/** Prototypes */
char *create_page(void);

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
static void ICACHE_FLASH_ATTR tcp_server_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
	struct station_config station_info;
	char *p1, *p2, *p3;
	//received some data from tcp connection

	struct espconn *pespconn = arg;
	os_printf("tcp recv : %s \r\n", pusrdata);

	// Here we get the data back, so let's find the network and the password.
	// === wifi network ===
	p1 = strstr(pusrdata, "network");

	if (p1 == 0)
		return;

	memset(&station_info, 0, sizeof(station_info));  //set value of config from address of &config to width of size to be value '0'

	for (; *p1 != '='; p1++)
		;						// FIXME: may go too far
	p1++;
	p2 = p1;
	for (; *p1 != '&'; p1++)
		;						// FIXME: may go too far
	p3 = p1;
	memcpy(station_info.ssid, p2, p3 - p2);
	station_info.ssid[p3 - p2] = 0;			// FIXME: check if needed
	// === password ===
	for (; *p1 != '='; p1++)
		;						// FIXME: may go too far
	p1++;
	p2 = p1;
	for (; *p1 != 0; p1++)
		;						// FIXME: may go too far
	p3 = p1;
	memcpy(station_info.password, p2, p3 - p2);
	station_info.password[p3 - p2] = 0;		// FIXME: check if needed

	// === Send info for main task ===
	xQueueSend(network_queue, &station_info, 0);
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

