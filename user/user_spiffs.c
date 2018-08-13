/*
 * user_spiffs.c
 *
 *  Created on: 10 juil. 2018
 *      Author: gautier
 */

#include "esp_common.h"
#include "spiffs.h"

#include <fcntl.h>

#define FS1_FLASH_SIZE      (1000*1024)				// Size of SPIFFS image 0xfa000
#define FS1_FLASH_ADDR      (3*1024*1024)			// Start of SPIFFS image in FLASH memory space 0x300000
#define SECTOR_SIZE         (8*1024)
#define LOG_BLOCK           (SECTOR_SIZE)			// Size of a logical block
#define LOG_PAGE            (256)					// Size of a logical page
#define FD_BUF_SIZE         32*4
#define CACHE_BUF_SIZE      (LOG_PAGE + 32)*8

void spiffs_fs1_init(void)
{
	s32_t ret;
	struct esp_spiffs_config config;

	config.phys_size = FS1_FLASH_SIZE;
	config.phys_addr = FS1_FLASH_ADDR;
	config.phys_erase_block = SECTOR_SIZE;
	config.log_block_size = LOG_BLOCK;
	config.log_page_size = LOG_PAGE;
	config.fd_buf_size = FD_BUF_SIZE * 2;
	config.cache_buf_size = CACHE_BUF_SIZE;

	ret = esp_spiffs_init(&config);

	if (ret)
		os_printf("ERR: Can't init spiffs image!\n");

}

void user_spiffs()
{
	char out[20] = { 0 };
	int pfd;
	int res;

	// Mount SPIFFS image
	spiffs_fs1_init();
}
