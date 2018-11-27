/*
 * user_tcp_client_ssl.c
 *
 *  Created on: 12 nov. 2018
 *      Author: gautier
 */
#include "esp_common.h"

#include "ssl/ssl_ssl.h"

/**
 * Implement the SSL client logic.
 */
static void ICACHE_FLASH_ATTR esp_client_ssl_thread(void *pTaskParamer)
{

	uint32 options = SSL_DISPLAY_CERTS | SSL_NO_DEFAULT_KEY;
	SSL_CTX *ssl_ctx = NULL;

	os_printf("Start and Running the task %d\n", system_get_free_heap_size());

	/*create the client context*/
	while (ssl_ctx == NULL) {
		ssl_ctx = ssl_ctx_new(options, SSL_DEFAULT_CLNT_SESS);
	}
	ssl_obj_memory_load(ssl_ctx, SSL_OBJ_X509_CACERT, default_certificate, default_certificate_len, NULL);

	/*************************************************************************
	 * This is where the interesting stuff happens. Up until now we've
	 * just been setting up sockets etc. Now we do the SSL handshake.
	 *************************************************************************/
	while (1) {
		int client_fd;
		struct sockaddr_in client_addr;
		uint32 sin_addr;
		uint16 port = 0;
		struct hostent *hostent;
		SSL *ssl = NULL;
		int res;
		uint8 *read_buf = NULL;
		int recbytes;

		port = parame->port;
		if (parame->host != NULL) {
			do {
				os_printf("Error: get host by name is invalid\n");
				hostent = gethostbyname(parame->host);
			} while (hostent == NULL);
			sin_addr = *((uint32_t **) hostent->h_addr_list)[0];
		} else {
			sin_addr = parame->ip.addr;
		}

		os_printf("start heap size %d\n", system_get_free_heap_size());
		client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (client_fd < 0) {
			os_printf("create the socket err\n");
			goto finish;
		}
		memset(&client_addr, 0, sizeof(client_addr));
		client_addr.sin_family = AF_INET;
		client_addr.sin_port = htons(port);
		client_addr.sin_addr.s_addr = sin_addr;

		if (connect(client_fd, (struct sockaddr *) &client_addr, sizeof(client_addr)) < 0) {
			os_printf("connect with the host err\n");
			goto finish;
		}

		ssl = ssl_client_new(ssl_ctx, client_fd, NULL, 0);
		if (ssl == NULL)
			goto finish;

		if ((res = ssl_handshake_status(ssl)) == SSL_OK) {
			os_printf("handshake ok\n");
			x509_free(ssl->x509_ctx);
			ssl->x509_ctx = NULL;
			uint8 buf[512];
			write_again: bzero(buf, sizeof(buf));
			sprintf(buf, Ping_Fragme);

			res = ssl_write(ssl, buf, strlen(buf));
			if (res < 0)
				goto finish;
			else {
				while ((recbytes = ssl_read(ssl, &read_buf)) >= 0) {
					if (recbytes == 0) {
						espssl_sleep(500);
						continue;
					} else {
						os_printf("recbytes %d, %s\n", recbytes, read_buf);
						if (strcmp(read_buf, "ping success") != 0) {
							espssl_sleep(500);
							goto write_again;
						} else
							break;
					}
				}
			}
		} else {
			os_printf("handshake status %d\n", ssl_handshake_status(ssl));
		}
finish: ssl_free(ssl);
		close(client_fd);
		espssl_sleep(500);
	}

	ssl_ctx_free(ssl_ctx);
	os_printf("Exit and delete the task %d\n", system_get_free_heap_size());
	free(parame);
	vTaskDelete(NULL);
}

void ICACHE_FLASH_ATTR user_tcpclient_ssl_init(void)
{
	int ret;

	ret = xTaskCreate(esp_client_ssl_thread, "esp client ssl task", 1024, NULL, 4, NULL);

	if (ret != pdPASS) {
		os_printf("esp ssl client thread failed\n");
	} else {
		os_printf("esp ssl client thread succeeded\n");
	}

}
