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

/* Constants */
static const char html_fmt[] ICACHE_RODATA_ATTR STORE_ATTR =
		"HTTP/1.1 200 OK\r\n"
		"Content-length: %d\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"%s";

/* Functions */
/**
 * Process the html header data which are pushed to TCP server.
 * Target is to know which method (GET ou POST) and page are requested.
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
		size = sizeof(content->page)-1;
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
