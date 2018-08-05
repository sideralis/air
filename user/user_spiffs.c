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

//	// Create a new file
//	char *buf = "hello world";
//
//	pfd = open("myfile", O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
//	if (pfd <= 3) {
//		printf("open file error\n");
//	}
//
//	// Write to new file
//	int write_byte = write(pfd, buf, strlen(buf));
//	if (write_byte <= 0) {
//		printf("write file error\n");
//	}
//	close(pfd);
//
//	// Read previous created file
//	pfd = open("myfile", O_RDWR);
//	if (pfd < 3)
//		os_printf("ERR: Can't open file! %d\n", pfd);
//	else {
//		if (read(pfd, out, 20) < 0)
//			os_printf("ERR: Can't read file!\n");
//		close(pfd);
//		printf("--> %s <--\n", out);
//	}
//
//	// Read file from generated image with mkspiffs tool
//	pfd = open("/readme.txt", O_RDWR);			// !! Don't forget to add the / before the name of the file
//	if (pfd < 3)
//		os_printf("ERR: Can't open file! %d\n", pfd);
//	else {
//		if (read(pfd, out, 20) < 0)
//			os_printf("ERR: Can't read file!\n");
//		close(pfd);
//		printf("--> %s <--\n", out);
//	}
}