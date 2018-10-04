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

void extract_param(char *str)
{
	pParam = strchr(pQuery,'&');
	while (pParam != 0) {
		pSep = strchr()
	}

}
/**
 * Process the html header data which are pushed to TCP server.
 * Target is to know which method (GET ou POST) and page are requested.
 */
#define CONTENT_TYPE 	"Content-Type: "
#define CONTENT_LENGTH	"Content-Length: "
int IRAM_ATTR process_header_recv(char *pusrdata, struct header_html_recv *request)
{
	char *pGP, *pHTTP, *pQuery, *pParam, *pContentType, *pContentLength, *pEnd;
	int size;
	char page[128];
	int query_nb = 0;

	// Let's clear request
	request->method = 0;
	request->page_name = 0;
	for (i=0;i<sizeof(request->form)/sizeof(request->form[0]);i++) {
		request->form[i].key = 0;
		request->form[i].valure = 0;
	}

	// First find method used
	pGP = strstr(pusrdata,"GET");
	if (pGP != 0) {
		request->method = METHOD_GET;
		pGP += 4;
	} else {
		pGP = strstr(pusrdata,"POST");
		if (pGP == 0)
			return -1;
		request->method = METHOD_POST;
		pGP += 5;
	}
	// Now get page name
	pHTTP = strstr(pusrdata,"HTTP");
	if (pHTTP == 0 || pHTTP < pGP +1)
		return -1;
	size = pHTTP-pGP-1;
	if (size > sizeof(page))
		size = sizeof(page)-1;
	memcpy(content->page, pGP, size);
	request->page[size] = 0;

	// Now get parameter key and value
	if (request->method == METHOD_GET) {
		pQuery = strchr(page,'?');
		if (pQuery == 0) {
			// No parameter, just copy the page name.
			page[31] = 0;
			strcpy(page,request->page_name);
		} else {
			pEnd = strchr(pQuery,'#');
			if (pEnd == 0)
				// To continue find the length.
			extract_param(pQuery+1,,request->form);
		}
	} else {
		// Search for Content-type field
		pContentType = strstr(pusrdata,CONTENT_TYPE);
		if (pContentType == 0)
			return -1;
		if (strcmp(pContentType+sizeof(CONTENT_TYPE),"application/x-www-form-urlencoded") != 0)
			return -1;			// Error type is 415
		// Search for content length
		pContentLength = strstr(pusrdata,CONTENT_LENGTH);
		if (pContentLength == 0)
			return -1;
		pContentLength += sizeof(CONTENT_LENGTH);
		pEnd = strstr(pContentLength, "\r\n");
		if (pEnd == 0)
			return -1;
		strncpy(page,pContentLength,pEnd-pContentLength);
		page[pEnd-pContentLength] = 0;
		size = atoi(page);
		// Search for parameters, e.g. for an empty line
		pParam = strstr(pContentLength,"\r\n\r\n");
		if (pParam == 0) {
			return -1;
		}
		pParam += 2;

		extract_param(pParam, size, request->form);
	}
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
