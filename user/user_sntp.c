/*
 * user_sntp.c
 *
 *  Created on: 2 déc. 2018
 *      Author: gautier
 */

#include "esp_common.h"
#include "freertos/task.h"

#include "lwip/apps/time.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/sntp_time.h"
#include "lwip/apps/sntp_opts.h"
#include "lwip/udp.h"

#define SNTP_SERVERS       "pool.ntp.org"

char* getrealtimeofday(void);

void IRAM_ATTR stnp_start(void)
{
	uint32_t current_timestamp = 0;
	const portTickType xDelay = 500 / portTICK_RATE_MS;
	char *timess;

	time_t now;
	sntp_tm timeinfo;
	time(&now);

	os_printf("INFO: SNTP init\n");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, SNTP_SERVERS);
	sntp_set_timezone(0);						// UTC
	sntp_init();

	vTaskDelay(5000 / portTICK_RATE_MS);
	os_printf("INFO: SNTP waiting for time set\n");
	do {
		current_timestamp = sntp_get_current_timestamp();
		vTaskDelay(xDelay);
	} while (current_timestamp == 0);

	time(&now);

	os_printf("INFO: SNTP get time\n");
	timess = getrealtimeofday();
	os_printf("INFO: Time of today is %s\n", timess);
}
