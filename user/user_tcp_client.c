/*
 * user_tcp_client.c
 *
 *  Created on: 23 oct. 2018
 *      Author: gautier
 */

#include "esp_common.h"
#include "espconn.h"

#include "lwip/netif.h"

#include "user_html.h"

#define pheadbuffer "GET /device?id=%s&mac=%0x%0x%0x%0x%0x%0x HTTP/1.1\r\nHost: %d.%d.%d.%d\r\nUser-Agent: curl/7.37.0\r\nAccept: */*\r\n\r\n"

#define packet_size   (2 * 1024)

static struct espconn user_tcp_conn;
static esp_tcp user_tcp;
ip_addr_t tcp_server_ip;

extern char user_id[MAX_LENGHT_USER_ID];
extern u8_t mac[NETIF_MAX_HWADDR_LEN];
extern struct ip_addr my_ip;

/******************************************************************************
 * FunctionName : user_tcp_recv_cb
 * Description  : receive callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
static void ICACHE_FLASH_ATTR
user_tcp_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
	//received some data from tcp connection

	os_printf("INFO: tcp client data receive %c%c%c%c%c%c%c%c...\n", pusrdata[0], pusrdata[1], pusrdata[2], pusrdata[3], pusrdata[4], pusrdata[5], pusrdata[6],
			pusrdata[7]);

}
/******************************************************************************
 * FunctionName : user_tcp_sent_cb
 * Description  : data sent callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
static void ICACHE_FLASH_ATTR
user_tcp_sent_cb(void *arg)
{
	//data sent successfully

	os_printf("INFO: tcp client sent callback: data sent successfully\n");
}
/******************************************************************************
 * FunctionName : user_tcp_discon_cb
 * Description  : disconnect callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
static void ICACHE_FLASH_ATTR
user_tcp_discon_cb(void *arg)
{
	//tcp disconnect successfully

	os_printf("INFO: tcp client disconnected from server\n");
}
/******************************************************************************
 * FunctionName : user_esp_platform_sent
 * Description  : Processing the application data and sending it to the host
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
 *******************************************************************************/
static void ICACHE_FLASH_ATTR
user_send_data(struct espconn *pespconn)
{
	sint8 ret;
	char *pbuf = (char *) os_zalloc(packet_size);

	sprintf(pbuf, pheadbuffer, user_id, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], pespconn->proto.tcp->remote_ip[0], pespconn->proto.tcp->remote_ip[1],
			pespconn->proto.tcp->remote_ip[2], pespconn->proto.tcp->remote_ip[3]);

	ret = espconn_send(pespconn, pbuf, strlen(pbuf));

	if (ret)
		os_printf("Error in sending message %i\n", ret);

	os_free(pbuf);
}

/******************************************************************************
 * FunctionName : user_tcp_connect_cb
 * Description  : A new incoming tcp connection has been connected.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
static void ICACHE_FLASH_ATTR
user_tcp_connect_cb(void *arg)
{
	struct espconn *pespconn = arg;

	os_printf("INFO: tcp client connected to server\n");

	espconn_regist_recvcb(pespconn, user_tcp_recv_cb);
	espconn_regist_sentcb(pespconn, user_tcp_sent_cb);
	espconn_regist_disconcb(pespconn, user_tcp_discon_cb);

	user_send_data(pespconn);
}

/******************************************************************************
 * FunctionName : user_tcp_recon_cb
 * Description  : reconnect callback, error occured in TCP connection.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
static void ICACHE_FLASH_ATTR
user_tcp_recon_cb(void *arg, sint8 err)
{
	//error occured , tcp connection broke. user can try to reconnect here.
	os_printf("INFO: tcp client reconnect callback called, error code: %d\n", err);
}

/******************************************************************************
 * FunctionName : user_check_ip
 * Description  : check whether get ip addr or not
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void ICACHE_FLASH_ATTR user_tcpclient_init(void)
{
	// Connect to tcp server with our ip address
	user_tcp_conn.proto.tcp = &user_tcp;
	user_tcp_conn.type = ESPCONN_TCP;
	user_tcp_conn.state = ESPCONN_NONE;

	const char esp_tcp_server_ip[4] = AIR_SERVER_IP; 	// remote IP of TCP server

	memcpy(user_tcp_conn.proto.tcp->remote_ip, esp_tcp_server_ip, 4);
	user_tcp_conn.proto.tcp->remote_port = 80;  				// remote port
	user_tcp_conn.proto.tcp->local_port = espconn_port(); 		//local port of ESP8266

	espconn_regist_connectcb(&user_tcp_conn, user_tcp_connect_cb); // register connect callback
	espconn_regist_reconcb(&user_tcp_conn, user_tcp_recon_cb); // register reconnect callback as error handler
	espconn_connect(&user_tcp_conn);
}

