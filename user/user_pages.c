/*
 * user_pages.c
 *
 *  Created on: 11 oct. 2018
 *      Author: gautier
 */


#include "esp_common.h"
#include "espconn.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "user_html.h"
#include "user_queue.h"


int page_wifi(struct header_html_recv *request, struct espconn *pesp_conn)
{
	return html_render_template(request->page_name, pesp_conn);
}
/**
 * Process the choice of the network by the user
 */
int page_connect(struct header_html_recv *request, struct espconn *pesp_conn)
{
	// TODO: test with an empty password

	struct station_config station_info;

	memset(&station_info, 0, sizeof(station_info));  //set value of config from address of &config to width of size to be value '0'

	strcpy(station_info.ssid, request->form[0].value);
	strcpy(station_info.ssid, request->form[1].value);

	// Send info for main task
	xQueueSend(network_queue, &station_info, 0);

	return html_render_template(request->page_name, pesp_conn);
}
