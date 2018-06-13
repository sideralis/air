/*
 * user_html.c
 *
 *  Created on: 10 avr. 2018
 *      Author: gautier
 */
#include "esp_common.h"
#include "freertos/queue.h"

/* Defines */
#define	GET_ALIGN_STRING_LEN(str) ((strlen(str) + 4) & ~3)

/* External variables */
extern xQueueHandle wifi_scan_queue;

/* Constants */
static const char html_fmt[] ICACHE_RODATA_ATTR =
		"HTTP/1.1 200 OK\r\n"
		"Content-length: %d\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"%s%s";

static const char html_cfg[] ICACHE_RODATA_ATTR =
		"<html>\r\n"
		"<head>\r\n"
		"    <title>AIR</title>\r\n"
		"    <meta http-equiv=\"refresh\" content=\"10\">\r\n"
		"</head>\r\n";

static const char html_data_1[] ICACHE_RODATA_ATTR =
		"<body>\r\n"
		"<article>\r\n"
		"    <section>\r\n"
		"        <h1>Networks</h1>\r\n";

static const char html_data_2[] ICACHE_RODATA_ATTR =
		"    </section>\r\n"
		"    <footer>\r\n"
		"        <ul>\r\n"
		"            <li>@BG</li>\r\n"
		"        </ul>\r\n"
		"    </footer>\r\n"
		"</article>\r\n"
		"</body>\r\n"
		"</html>\r\n";

static const char wifi_authmode[AUTH_MAX][16] ICACHE_RODATA_ATTR = {
		"open", "wep", "wpa_psk", "wpa2_psk", "wpa_wpa2_psk"
};

#define MAX_INFO_SIZE 64

/* Functions */
char *create_wifi_list(void) {
	signed portBASE_TYPE ret;

	struct bss_info *wifi_scan;
	char (*wifi_info)[MAX_INFO_SIZE];
	int wifi_count = 0;
	char (*wifi)[16];															// FIXME: hard coded values AUTH_MAX x 16
	int i;

	wifi = (char (*)[16])malloc(AUTH_MAX*16);									// FIXME: hard coded values!
	wifi_info = (char (*)[MAX_INFO_SIZE])malloc(10*MAX_INFO_SIZE);				// FIXME: hard coded values!
	if (wifi == 0 || wifi_info == 0) {
		os_printf("Cannot allocate memory to send html answer!\n");
		return 0;
	}
	system_get_string_from_flash(wifi_authmode, wifi, AUTH_MAX*16);

	/* Loop to get all wifi networks and fill the wifi_info table */
	do {
		ret = xQueueReceive(wifi_scan_queue, &wifi_scan, 0);
		if (ret == errQUEUE_EMPTY)
			break;		// Exit from loop if no more network
		sprintf(wifi_info[wifi_count++],"<p>%s %d %d</p>\r\n",wifi_scan->ssid, wifi_scan->authmode, wifi_scan->rssi);	// FIXME: check authmode is in range
		free(wifi_scan);		// Release the data as we don't need them anymore
	} while (wifi_count<10);

	os_printf("__DBG__ %d\n",wifi_count);	/* DBG */

	/* Complete the fill_info table */
	while(wifi_count<10) {
		strcpy(wifi_info[wifi_count++],"<p>------</p>\r\n");
	}
	/* DBG */
	os_printf("__DBG__ %d\n",wifi_count);
	for (i=0;i<10;i++) {
		os_printf("__DBG__ %s\n",wifi_info[i]);
	}
	/* DBG END */

	char *data_1, *data_2, *data_wifi;
	int data_1_size = GET_ALIGN_STRING_LEN(html_data_1);
	int data_2_size = GET_ALIGN_STRING_LEN(html_data_2);

	data_1 = (char *) malloc(data_1_size);
	data_2 = (char *) malloc(data_2_size);
	data_wifi = (char *)malloc(data_1_size + data_2_size + 10*64);		// FIXME: hard coded values!
	if (data_1 == 0 || data_2 == 0 || data_wifi == 0) {					// FIXME: should check one by one and not all in one shot
		os_printf("Cannot allocate memory to send html answer!\n");
		return 0;
	}

	system_get_string_from_flash(html_data_1, data_1, data_1_size);
	system_get_string_from_flash(html_data_2, data_2, data_2_size);

	strcpy(data_wifi,data_1);
	for (i = 0 ;i < 10;i++) {
		strcat(data_wifi, wifi_info[i]);
	}
	strcat(data_wifi, data_2);

	os_printf("==DBG1==:\n%s\n",data_1);

	free(data_1);
	free(data_2);
	free(wifi_info);
	free(wifi);

	return data_wifi;
}

/**
 * Called by tcp server
 */
char *create_page(void) {

	char *fmt, *cfg, *data;
	char *m;

	// DBG
	unsigned long nbTask;
	void *h;
	nbTask = uxTaskGetNumberOfTasks();
	os_printf("DBG_html: Nb of tasks = %d\n",nbTask);
	h = xTaskGetCurrentTaskHandle();
	os_printf("DBG_html: handle = %p\n",h);
	// END DBG

	// Create the
	int fmtsize = GET_ALIGN_STRING_LEN(html_fmt);
	int cfgsize = GET_ALIGN_STRING_LEN(html_cfg);

	m = (char *) malloc(1024);							// FIXME: hard coded values!

	fmt = (char *) malloc(fmtsize);
	cfg = (char *) malloc(cfgsize);
	if (m == 0 || fmt == 0 || cfg == 0) {	// FIXME: should check one by one and not all in one shot
		os_printf("Cannot allocate memory to send html answer!\n");
		return 0;
	}
	system_get_string_from_flash(html_fmt, fmt, fmtsize);
	system_get_string_from_flash(html_cfg, cfg, cfgsize);

	data = create_wifi_list();
//	os_printf("DBG: %s\n",data);
	sprintf(m,fmt,strlen(cfg)+strlen(data),cfg,data);

//	char data1[] = "<body><h1>TITRE</h1></body></html>\r\n";
//	sprintf(m,fmt,strlen(cfg)+strlen(data1),cfg,data1);

	free(fmt);
	free(data);
	free(cfg);

	return m;

}
