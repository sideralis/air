/*
 * user_html.c
 *
 *  Created on: 10 avr. 2018
 *      Author: gautier
 */
#include "esp_common.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "user_wifi.h"
#include "user_html.h"
#include "user_queue.h"

/* Defines */
#define	GET_ALIGN_STRING_LEN(str) ((strlen(str) + 4) & ~3)

/* External variables */
extern xQueueHandle wifi_scan_queue;
extern SLIST_HEAD(router_info_head, router_info) router_data;

/* Constants */
static const char html_fmt[] ICACHE_RODATA_ATTR STORE_ATTR =
		"HTTP/1.1 200 OK\r\n"
		"Content-length: %d\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"%s";

static const char html_cfg[] ICACHE_RODATA_ATTR STORE_ATTR =
		"<html>\r\n"
		"<head>\r\n"
		"    <title>AIR</title>\r\n"
		"</head>\r\n";

static const char html_data_1[] ICACHE_RODATA_ATTR STORE_ATTR =
		"<body>\r\n"
		"<form action=\"/wifi.html\" method=\"post\">\r\n"
		"  WIFI network:<br>\r\n"
		"  <select name=\"network\">\r\n";

static const char html_data_2[] ICACHE_RODATA_ATTR STORE_ATTR =
		"  </select>\r\n"
		"  <br>\r\n"
		"  WIFI password:<br>\r\n"
		"  <input type=\"password\" name=\"psw\">\r\n"
		"  <br><br>\r\n"
		"  <input type=\"submit\" value=\"Send\">\r\n"
		"</form>\r\n"
		"</body>\r\n"
		"</html>\r\n";

static const char html_choosen[] ICACHE_RODATA_ATTR STORE_ATTR =
		"<html>\r\n"
		"  <head>\r\n"
		"    <meta http-equiv=\"refresh\" content=\"5\""
		"  </head>\r\n"
		"  <body>\r\n"
		"    <p>Trying to connect to network. Please wait...</p>\r\n"
		"  </body>\r\n"
		"</html>\r\n";

static const char wifi_authmode[AUTH_MAX][16] ICACHE_RODATA_ATTR = { "open", "wep", "wpa_psk", "wpa2_psk", "wpa_wpa2_psk" };

#define MAX_WIFI_NETWORK 	10

/* Functions */

/**
 * Process the html header data which are pushed to TCP server.
 * Target is to know which method (GET ou PUSH) and page are requested.
 */
int IRAM_ATTR process_header_recv(char *pusrdata, struct header_html_recv *content)
{
	char *pGP;
	char *pHTTP;
	int size;

	pGP = strstr(pusrdata,"GET");
	if (pGP != 0) {
		content->get = 1;
		content->post = 0;
		pGP += 4;
	} else {
		pGP = strstr(pusrdata,"POST");
		if (pGP == 0)
			return -1;
		content->get = 0;
		content->post = 1;
		pGP += 5;
	}
	pHTTP = strstr(pusrdata,"HTTP");
	if (pHTTP == 0 || pHTTP < pGP +1)
		return -1;
	size = pHTTP-pGP-1;
	if (size > sizeof(content->page))
		size = 31;
	memcpy(content->page, pGP, size);
	content->page[size] = 0;

	return 0;
}
/**
 * Process the choice of the network by the user
 */
int IRAM_ATTR process_network_choice(char *pusrdata)
{
	// TODO: test with an empty password

	struct station_config station_info;
	char *p1, *p2, *p3;

	// === wifi network ===
	p1 = strstr(pusrdata, "network");

	if (p1 == 0)
		return -1;

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
	for (; *p1 != 0 && *p1 != '&'; p1++)
		;						// FIXME: may go too far
	p3 = p1;
	memcpy(station_info.password, p2, p3 - p2);
	station_info.password[p3 - p2] = 0;		// FIXME: check if needed

	// === Send info for main task ===
	xQueueSend(network_queue, &station_info, 0);

	return 0;
}
/**
 * Create a list of available wifi network
 */
static char *create_wifi_list(void)
{
	char *wifi_option;
	char *wifi_list;

	int i;

	int data1_size, data2_size;
	char *data1, *data2;
	char *data;

	struct router_info *info = NULL;

	wifi_option = zalloc(40 + 33 + 33);									// FIXME: check if zero
	wifi_list = zalloc(MAX_WIFI_NETWORK * (40 + 33 + 33));				// FIXME: check if zero

	i = 0;
	SLIST_FOREACH(info, &router_data, next)
	{
		sprintf(wifi_option, "    <option value=\"%s\">%s</option>\r\n", info->ssid, info->ssid);
		strcat(wifi_list, wifi_option);
		if (++i == MAX_WIFI_NETWORK)
			break;
	}

	data1_size = GET_ALIGN_STRING_LEN(html_data_1);
	data2_size = GET_ALIGN_STRING_LEN(html_data_2);

	data1 = (char *) malloc(data1_size);									// FIXME: check if zero
	data2 = (char *) malloc(data2_size);									// FIXME: check if zero
	data = (char *) malloc(data1_size + strlen(wifi_list) + data2_size);	// FIXME: check if zero

	system_get_string_from_flash(html_data_1, data1, data1_size);
	system_get_string_from_flash(html_data_2, data2, data2_size);

	sprintf(data, "%s%s%s", data1, wifi_list, data2);

	return data;
}

IRAM_ATTR char *html_add_header(char *page)
{
	char *fmt;
	char *m;

	int fmtsize = GET_ALIGN_STRING_LEN(html_fmt);
	fmt = (char *) malloc(fmtsize);
	if (fmt == 0) {
		os_printf("Cannot allocate memory to send html answer!\n");
		return 0;
	}
	system_get_string_from_flash(html_fmt, fmt, fmtsize);

	m = (char *) malloc(fmtsize + strlen(page) + 2);			// FIXME: check if zero + include size of data in malloc!!
	sprintf(m, fmt, strlen(page), page);

	free(fmt);

	return m;
}
/**
 * Called by tcp server
 */
char *display_network_choice(void)
{

	char *fmt, *cfg, *data;
	char *m;

	// Create the body of the page
	data = create_wifi_list();

	// Create the header and title
	int fmtsize = GET_ALIGN_STRING_LEN(html_fmt);
	int cfgsize = GET_ALIGN_STRING_LEN(html_cfg);
	fmt = (char *) malloc(fmtsize);
	cfg = (char *) malloc(cfgsize);
	if (fmt == 0 || cfg == 0) {	// FIXME: should check one by one and not all in one shot
		os_printf("Cannot allocate memory to send html answer!\n");
		return 0;
	}
	system_get_string_from_flash(html_fmt, fmt, fmtsize);
	system_get_string_from_flash(html_cfg, cfg, cfgsize);

	// Create the full page
	m = (char *) malloc(fmtsize + cfgsize + strlen(data) + 2);			// FIXME: check if zero + include size of data in malloc!!
	sprintf(m, fmt, strlen(cfg) + strlen(data), cfg, data);

	free(fmt);
	free(data);
	free(cfg);

	return m;

}

char IRAM_ATTR *display_network_choosen(void)
{
	char *m;
	char *fmt, *data;

	int fmtsize = GET_ALIGN_STRING_LEN(html_fmt);
	int datasize = GET_ALIGN_STRING_LEN(html_choosen);
	fmt = (char *) malloc(fmtsize);
	data = (char *)malloc(datasize);
	if (data == 0 || fmt == 0) {
		os_printf("Cannot allocate memory to send html answer!\n");		// FIXME: should free some maybe
		return 0;
 	}
	system_get_string_from_flash(html_fmt, fmt, fmtsize);
	system_get_string_from_flash(html_choosen, data, datasize);

	// Create the final page
	m = (char *) malloc(fmtsize + datasize + 2);			// FIXME: verify that it works for a length of data on 4 bytes!!
	if (m == 0) {
		os_printf("Cannot allocate memory to send html answer!\n");
		return 0;
	}
	sprintf(m, fmt, strlen(data), data, "");

	free(fmt);
	free(data);

	return m;
}

char *display_404(void)
{
	os_printf("ERR: Requested page does not exist!\n");
}
