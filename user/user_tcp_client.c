/*
 * user_tcp_client.c
 *
 *  Created on: 23 oct. 2018
 *      Author: gautier
 */

#include "esp_common.h"
#include "espconn.h"

#include "lwip/netif.h"
#include "lwip/netdb.h"

#include "user_html.h"
#include "user_device.h"

//#define DNS_ENABLE

#ifdef DNS_ENABLE
#define pheadbuffer "GET /device?id=%s&mac=%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: curl/7.37.0\r\nAccept: */*\r\n\r\n"
#else
#define pheadbuffer "GET /device?id=%s&mac=%s HTTP/1.1\r\nHost: %d.%d.%d.%d\r\nUser-Agent: curl/7.37.0\r\nAccept: */*\r\n\r\n"
#endif

#define packet_size   (2 * 1024)

static struct espconn user_tcp_conn;
static esp_tcp user_tcp;
static ip_addr_t tcp_server_ip;
static os_timer_t test_timer;
static bool tcp_client_reconnect;

extern char user_id[MAX_LENGHT_USER_ID];
extern u8_t mac[NETIF_MAX_HWADDR_LEN];
extern struct ip_addr my_ip;

void user_tcpclient_init(void);

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

#ifdef DNS_ENABLE
	sprintf(pbuf, pheadbuffer, user_id, this_device.mac, AIR_SERVER_NAME);
#else
	sprintf(pbuf, pheadbuffer, user_id, this_device.mac, pespconn->proto.tcp->remote_ip[0], pespconn->proto.tcp->remote_ip[1],
			pespconn->proto.tcp->remote_ip[2], pespconn->proto.tcp->remote_ip[3]);
#endif
	ret = espconn_send(pespconn, pbuf, strlen(pbuf));

	if (ret)
		os_printf("Error in sending message %i\n", ret);

	os_free(pbuf);
}

/******************************************************************************
 * FunctionName : user_tcp_recv_cb
 * Description  : receive callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
static void ICACHE_FLASH_ATTR
user_tcp_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
	struct header_html_recv *request;
	struct espconn *pespconn = arg;

	request = malloc(sizeof(struct header_html_recv));
	if (request == 0) {
		os_printf("ERR: malloc %d %f\n", __LINE__, __FILE__);
		return;
	}
	//received some data from tcp connection
	os_printf("INFO: tcp client data receive:\n%s\n", pusrdata);
	process_header_ack(pusrdata, request);
	os_printf("DBG: status ack = %d\n",request->status);
	os_printf("DBG: content ack = %s\n", request->content);

	if (request->status == 200) {
		// TODO do a function and simplify
		char *pS, *pE;
		pS = strchr(request->content,'=');
		if (pS == 0) {
			free(request);
			return;
		}
		pS = pS + 1;
		pE = strstr(pS,"=]");

		if (request->content[1] == '0') {
			strncpy(this_device.token, pS, pE-pS);
			this_device.token[pE-pS] = 0;
		} else {
	//		os_printf("ERR: %s\n", request->content);
		}
		// FIXME if we receive a status of 200 but not with the right content, we should send the message again.
		tcp_client_reconnect = false;
	} else {
		// TODO send the registration data again if we don't get the right answer, else disconnect.
		os_printf("DBG: Will try to reconnect at disconnect\n");
		tcp_client_reconnect = true;
	}

	free(request);


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
	if (tcp_client_reconnect == true) {
		// FIXME a timeout is needed
		os_printf("DBG: Trying to reconnect\n");
		user_tcpclient_init();
	}
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
#ifdef DNS_ENABLE
/******************************************************************************
 * FunctionName : user_dns_found
 * Description  : dns found callback
 * Parameters   : name -- pointer to the name that was looked up.
 *                ipaddr -- pointer to an ip_addr_t containing the IP address of
 *                the hostname, or NULL if the name could not be found (or on any
 *                other error).
 *                callback_arg -- a user-specified callback argument passed to
 *                dns_gethostbyname
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
	struct espconn *pespconn = (struct espconn *)arg;

	if (ipaddr == NULL) {
		os_printf("user_dns_found NULL\n");

	    struct hostent* ipAddress;

	    if ((ipAddress = gethostbyname(AIR_SERVER_NAME)) == 0) {
			os_printf("user_dns_found still NULL\n");
	        return;
	    } else {
			os_printf("user_dns_found YES\n");
	    }

		return;
	}


	//dns got ip
	os_printf("user_dns_found %d.%d.%d.%d \r\n",
			*((uint8 *)&ipaddr->addr), *((uint8 *)&ipaddr->addr + 1),
			*((uint8 *)&ipaddr->addr + 2), *((uint8 *)&ipaddr->addr + 3));

	if (tcp_server_ip.addr == 0 && ipaddr->addr != 0) {
		// dns succeed, create tcp connection
		os_timer_disarm(&test_timer);
		tcp_server_ip.addr = ipaddr->addr;
		memcpy(pespconn->proto.tcp->remote_ip, &ipaddr->addr, 4);	// remote ip of tcp server which get by dns

		pespconn->proto.tcp->remote_port = 80;						// remote port of tcp server
		pespconn->proto.tcp->local_port = espconn_port();			// local port of ESP8266

		espconn_regist_connectcb(pespconn, user_tcp_connect_cb);	// register connect callback
		espconn_regist_reconcb(pespconn, user_tcp_recon_cb);		// register reconnect callback as error handler
		espconn_regist_disconcb(pespconn, user_tcp_discon_cb);

		espconn_connect(pespconn);// tcp connect
	}
}
/******************************************************************************
 * FunctionName : user_esp_platform_dns_check_cb
 * Description  : 1s time callback to check dns found
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_dns_check_cb(void *arg)
{
	err_t err;

	struct espconn *pespconn = arg;

	err = espconn_gethostbyname(pespconn, AIR_SERVER_NAME, &tcp_server_ip, user_dns_found); // recall DNS function
	os_printf("DBG: gethostbyname return value %d\n", err);

	os_timer_arm(&test_timer, 1000, 0);
}
#endif
/******************************************************************************
 * FunctionName : user_check_ip
 * Description  : check whether get ip addr or not
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void user_tcpclient_init(void)
{
	// Connect to tcp server with our ip address
	user_tcp_conn.proto.tcp = &user_tcp;
	user_tcp_conn.type = ESPCONN_TCP;
	user_tcp_conn.state = ESPCONN_NONE;
	err_t err;

	tcp_client_reconnect = true;

#ifdef DNS_ENABLE
	tcp_server_ip.addr = 0;
	err = espconn_gethostbyname(&user_tcp_conn, AIR_SERVER_NAME, &tcp_server_ip, user_dns_found); // DNS function
	os_printf("DBG: gethostbyname return value %d\n", err);

//	os_timer_setfn(&test_timer, (os_timer_func_t *)user_dns_check_cb, &user_tcp_conn);
//	os_timer_arm(&test_timer, 1000, 0);
#else
	const char esp_tcp_server_ip[4] = AIR_SERVER_IP; 	// remote IP of TCP server

	memcpy(user_tcp_conn.proto.tcp->remote_ip, esp_tcp_server_ip, 4);
	user_tcp_conn.proto.tcp->remote_port = 80;  				// remote port
	user_tcp_conn.proto.tcp->local_port = espconn_port(); 		//local port of ESP8266

	espconn_regist_connectcb(&user_tcp_conn, user_tcp_connect_cb); // register connect callback
	espconn_regist_reconcb(&user_tcp_conn, user_tcp_recon_cb); // register reconnect callback as error handler
	espconn_connect(&user_tcp_conn);
#endif
}

