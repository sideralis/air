/*
 * user_html.c
 *
 *  Created on: 10 avr. 2018
 *      Author: gautier
 */
#include "esp_common.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "espconn.h"
#include <fcntl.h>

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
 * Extract a key and a value from a string starting at start and ending at end
 * String is key=value
 * Results are stored in form
 */
int extract_key_value(char *start, char *end, struct page_param *form)
{
	char *pSep;

	pSep = strchr(start,'=');
	if (pSep == 0 || pSep >= end)
		return -1;
	if ((pSep-start) > sizeof(form->key)-1)
		return -1;
	strncpy(form->key,start,pSep-start);
	form->key[pSep-start] = 0;
	if ((end-(pSep+1)) > sizeof(form->value)-1)
		return -1;
	strncpy(form->value,pSep+1,end-(pSep+1));
	form->value[end-(pSep+1)] = 0;

	return 0;
}
/**
 * Extract several keys and values from a string starting at start and ending at end
 * String is key1=value1&key2=value2&key3=value3...
 * Results are stored in form
 */
int extract_params(char *start, char *end, struct page_param *form)
{
	char *pParam;
	int i = 0;
	int ret;

	pParam = strchr(start,'&');
	while (pParam != 0 && pParam < end) {
		ret = extract_key_value(start, pParam, &form[i]);
		if (ret)
			return ret;
		i += 1;
		start = pParam+1;
		pParam = strchr(start,'&');
	}
	// Last parameters
	ret = extract_key_value(start, end, &form[i]);
	return ret;
}
/**
 * Process the html header data which are pushed to TCP server.
 * Target is to know which method (GET ou POST), which page is requested and with which parameters.
 */
#define CONTENT_TYPE 		"Content-Type: "
#define CONTENT_TYPE_VALUE 	"application/x-www-form-urlencoded"
#define CONTENT_LENGTH		"Content-Length: "
#define MAX_SIZE_FOR_PAGE_AND_PARAMS	128
int process_header_recv(char *pusrdata, struct header_html_recv *request)
{
	char *pGP, *pHTTP, *pQuery, *pParam, *pContentType, *pContentLength, *pEnd;
	int size;
	int query_nb = 0;
	int i;

	char *page_and_params;

	page_and_params = malloc(MAX_SIZE_FOR_PAGE_AND_PARAMS);
	if (page_and_params == 0) {
		os_printf("ERR: malloc %d %s\n",__LINE__,__FILE__);
		return -1;
	}

	// Let's clear request
	request->method = 0;
	request->page_name[0] = 0;
	for (i=0;i<sizeof(request->form)/sizeof(request->form[0]);i++) {
		request->form[i].key[0] = 0;
		request->form[i].value[0] = 0;
	}

	// First find method used
	pGP = strstr(pusrdata,"GET");
	if (pGP != 0) {
		request->method = METHOD_GET;
		pGP += 4;
	} else {
		pGP = strstr(pusrdata,"POST");
		if (pGP == 0) {
			free(page_and_params);
			return -1;
		}
		request->method = METHOD_POST;
		pGP += 5;
	}
	// Now get page name
	pHTTP = strstr(pusrdata,"HTTP");
	if (pHTTP == 0 || pHTTP < pGP +1) {
		free(page_and_params);
		return -1;
	}
	size = pHTTP-pGP-1;
	if (size > MAX_SIZE_FOR_PAGE_AND_PARAMS)
		size = MAX_SIZE_FOR_PAGE_AND_PARAMS-1;
	memcpy(page_and_params, pGP, size);
	page_and_params[size] = 0;

	// Now get parameter key and value
	if (request->method == METHOD_GET) {
		// Method GET: parameters are in the page name after character '?'
		pQuery = strchr(page_and_params,'?');
		if (pQuery == 0) {
			// No parameter, just copy the page name.
			page_and_params[sizeof(request->page_name)-1] = 0;			// Page name can't be longer than 31 characters
			strcpy(request->page_name, page_and_params);
		} else {
			// Some parameter, just copy the page name.
			strncpy(request->page_name, page_and_params, pQuery-page_and_params);
			request->page_name[pQuery-page_and_params] = 0;

			// Now let's find the end of the query
			pEnd = strchr(pQuery,'#');
			if (pEnd == 0) {
				pEnd = pQuery+strlen(pQuery);
			}
			// To continue find the length.
			extract_params(pQuery+1,pEnd,request->form);
		}
	} else {
		// Method POST: parameters are in data
		// First store the page name
		strcpy(request->page_name, page_and_params);
		// Search for Content-type field
		pContentType = strstr(pusrdata,CONTENT_TYPE);
		if (pContentType != 0) {
			if (strncmp(pContentType+sizeof(CONTENT_TYPE)-1,CONTENT_TYPE_VALUE, sizeof(CONTENT_TYPE_VALUE)-1) != 0) {
				free(page_and_params);
				return -1;			// Error type is 415
			}
		}
		// Search for content length
		pContentLength = strstr(pusrdata,CONTENT_LENGTH);
		if (pContentLength == 0) {
			free(page_and_params);
			return -1;
		}
		pContentLength += sizeof(CONTENT_LENGTH)-1;
		pEnd = strstr(pContentLength, "\r\n");
		if (pEnd == 0) {
			free(page_and_params);
			return -1;
		}
		strncpy(page_and_params,pContentLength,pEnd-pContentLength);
		page_and_params[pEnd-pContentLength] = 0;
		size = atoi(page_and_params);

		// Extract parameters from body if any
		if (size == 0) {
			if (pContentType != 0) {
				free(page_and_params);
				return -1;
			}
		} else {
			// Search for parameters, e.g. for an empty line
			pParam = strstr(pContentLength,"\r\n\r\n");
			if (pParam == 0) {
				free(page_and_params);
				return -1;
			}
			pParam += 4;		// to skip the empty line

			extract_params(pParam, pParam+size, request->form);
		}
	}
	free(page_and_params);
	return 0;
}

char *html_add_header(char *page)
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

int html_render_template(char *page_name, struct espconn *pesp_conn)
{
	int pfd;

	printTaskInfo();

	xSemaphoreTake( connect_sem, portMAX_DELAY );
	os_printf("INFO: Try opening page from spiffs %s\n", page_name);
	pfd = open(page_name, O_RDONLY);
	xSemaphoreGive( connect_sem );

	if (pfd < 3) {
		os_printf("ERR: Can't open HTML file! %s\n", page_name);
		return -1;
	} else {
		os_printf("INFO: Page is now open\n");

		// Read file
		char *page, *m;
		page = calloc(2048, 1);					// FIXME hard coded size
		if (page == 0) {
			os_printf("ERR: cannot allocate memory for page!\n");
			return -1;
		}

		if (read(pfd, page, 2048) < 0) {
			os_printf("ERR: Can't read file %s!\n", page_name);
			close(pfd);
			free(page);
			return -1;
		}
		close(pfd);

		// Add header
		m = html_add_header(page);
		free(page);

		// Return data
		espconn_send(pesp_conn, m, strlen(m));
		free(m);
		return 0;
	}
}
