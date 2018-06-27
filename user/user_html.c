/*
 * user_html.c
 *
 *  Created on: 10 avr. 2018
 *      Author: gautier
 */
#include "esp_common.h"
#include "freertos/queue.h"

#include "user_wifi.h"

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
		"%s%s";

static const char html_cfg[] ICACHE_RODATA_ATTR STORE_ATTR =
		"<html>\r\n"
		"<head>\r\n"
		"    <title>AIR</title>\r\n"
		"</head>\r\n";

static const char html_data_1[] ICACHE_RODATA_ATTR STORE_ATTR =
		"<body>\r\n"
		"<form method=\"post\">\r\n"
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

static const char wifi_authmode[AUTH_MAX][16] ICACHE_RODATA_ATTR = { "open", "wep", "wpa_psk", "wpa2_psk", "wpa_wpa2_psk" };

#define MAX_WIFI_NETWORK 	10

/* Functions */

/**
 * Create a list of available wifi network
 */
char *create_wifi_list2(void)
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


/**
 * Called by tcp server
 */
char *create_page(void)
{

	char *fmt, *cfg, *data;
	char *m;

	// Create the body of the page
	data = create_wifi_list2();

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
	m = (char *) malloc(fmtsize + cfgsize + strlen(data));			// FIXME: check if zero
	sprintf(m, fmt, strlen(cfg) + strlen(data), cfg, data);

	free(fmt);
	free(data);
	free(cfg);

	return m;

}
