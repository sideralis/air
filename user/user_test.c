/*
 * user_test.c
 *
 *  Created on: 5 oct. 2018
 *      Author: gautier
 */

#ifdef TEST

#include "esp_common.h"

#include "user_html.h"

char pusrdata_post1[] =
"POST /connect.html HTTP/1.1\r\n"
"Host: 192.168.4.1\r\n"
"User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:62.0) Gecko/20100101 Firefox/62.0\r\n"
"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
"Accept-Language: fr,fr-FR;q=0.8,en-US;q=0.5,en;q=0.3\r\n"
"Accept-Encoding: gzip, deflate\r\n"
"Referer: http://192.168.4.1/wifi.html\r\n"
"Content-Type: application/x-www-form-urlencoded\r\n"
"Content-Length: 48\r\n"
"Connection: keep-alive\r\n"
"Upgrade-Insecure-Requests: 1\r\n"
"\r\n"
"network=GUEST_BIBLIOTECA&psw=603036&send=Envoyer\r\n";

char pusrdata_post2[] =
"POST /wifilist.json HTTP/1.1\r\n"
"Host: 192.168.4.1\r\n"
"User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:62.0) Gecko/20100101 Firefox/62.0\r\n"
"Accept: */*\r\n"
"Accept-Language: fr,fr-FR;q=0.8,en-US;q=0.5,en;q=0.3\r\n"
"Accept-Encoding: gzip, deflate\r\n"
"Referer: http://192.168.4.1/wifi.html\r\n"
"Connection: keep-alive\r\n"
"Content-Length: 0\r\n"
"\r\n"
"\r\n";

char pusrdata_get1[] =
"GET /tutorials/other/top-20-mysql-best-practices/ HTTP/1.1\r\n"
"Host: net.tutsplus.com\r\n"
"User-Agent: Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US; rv:1.9.1.5) Gecko/20091102 Firefox/3.5.5 (.NET CLR 3.5.30729)\r\n"
"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
"Accept-Language: en-us,en;q=0.5\r\n"
"Accept-Encoding: gzip,deflate\r\n"
"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n"
"Keep-Alive: 300\r\n"
"Connection: keep-alive\r\n"
"Cookie: PHPSESSID=r2t5uvjq435r4q7ib3vtdjq120\r\n"
"Pragma: no-cache\r\n"
"Cache-Control: no-cache\r\n";

char pusrdata_get2[] =
"GET /foo.php?first_name=John&last_name=Doe&action=Submit HTTP/1.1\r\n"
"Host: net.tutsplus.com\r\n"
"User-Agent: Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US; rv:1.9.1.5) Gecko/20091102 Firefox/3.5.5 (.NET CLR 3.5.30729)\r\n"
"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
"Accept-Language: en-us,en;q=0.5\r\n"
"Accept-Encoding: gzip,deflate\r\n"
"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n"
"Keep-Alive: 300\r\n"
"Connection: keep-alive\r\n"
"Cookie: PHPSESSID=r2t5uvjq435r4q7ib3vtdjq120\r\n"
"Pragma: no-cache\r\n"
"Cache-Control: no-cache\r\n";

// Test the extract information from html header
int test_header_html_post1() {
	struct header_html_recv html_content;

	os_printf("Test hmtl header post with parameters:\n");

	process_header_recv(pusrdata_post1, &html_content);
	if (strcmp(html_content.page_name, "/connect.html") != 0)
		os_printf("Erreur nom de page\n");
	if (html_content.method != METHOD_POST)
		os_printf("Erreur methode\n");
	if (strcmp(html_content.form[0].key,"network") != 0)
		os_printf("Erreur clef parametre 1\n");
	if (strcmp(html_content.form[0].value,"GUEST_BIBLIOTECA") != 0)
		os_printf("Erreur valeur parametre 1\n");
	if (strcmp(html_content.form[1].key,"psw") != 0)
		os_printf("Erreur clef parametre 2\n");
	if (strcmp(html_content.form[1].value,"603036") != 0)
		os_printf("Erreur valeur parametre 2\n");
	if (strcmp(html_content.form[2].key,"send") != 0)
		os_printf("Erreur clef parametre 3\n");
	if (strcmp(html_content.form[2].value,"Envoyer") != 0)
		os_printf("Erreur valeur parametre 3\n");
}

int test_header_html_post2() {
	struct header_html_recv html_content;

	os_printf("Test hmtl header post w/o parameters:\n");

	process_header_recv(pusrdata_post2, &html_content);
	if (strcmp(html_content.page_name, "/wifilist.json") != 0)
		os_printf("Erreur nom de page\n");
	if (html_content.method != METHOD_POST)
		os_printf("Erreur methode\n");
	if (html_content.form[0].key[0] != 0)
		os_printf("Erreur clef parametre 1\n");
	if (html_content.form[0].value[0] != 0)
		os_printf("Erreur valeur parametre 1\n");
	if (html_content.form[1].key[0] != 0)
		os_printf("Erreur clef parametre 2\n");
	if (html_content.form[1].value[0] != 0)
		os_printf("Erreur valeur parametre 2\n");
}

int test_header_html_get1() {
	struct header_html_recv html_content;

	os_printf("Test html header get w/o parameters:\n");

	process_header_recv(pusrdata_get1, &html_content);
	if (strcmp(html_content.page_name, "/tutorials/other/top-20-mysql-best-practices/") != 0)
		os_printf("Erreur nom de page\n");
	if (html_content.method != METHOD_GET)
		os_printf("Erreur methode\n");
	if (html_content.form[0].key[0] != 0)
		os_printf("Erreur clef parametre 1\n");
	if (html_content.form[0].value[0] != 0)
		os_printf("Erreur valeur parametre 1\n");
	if (html_content.form[1].key[0] != 0)
		os_printf("Erreur clef parametre 2\n");
	if (html_content.form[1].value[0] != 0)
		os_printf("Erreur valeur parametre 2\n");
}

int test_header_html_get2() {
	struct header_html_recv html_content;

	os_printf("Test html header get with parameters:\n");

	process_header_recv(pusrdata_get2, &html_content);
	if (strcmp(html_content.page_name, "/foo.php") != 0)
		os_printf("Erreur nom de page\n");
	if (html_content.method != METHOD_GET)
		os_printf("Erreur methode\n");
	if (strcmp(html_content.form[0].key,"first_name") != 0)
		os_printf("Erreur clef parametre 1\n");
	if (strcmp(html_content.form[0].value,"John") != 0)
		os_printf("Erreur valeur parametre 1\n");
	if (strcmp(html_content.form[1].key,"last_name") != 0)
		os_printf("Erreur clef parametre 2\n");
	if (strcmp(html_content.form[1].value,"Doe") != 0)
		os_printf("Erreur valeur parametre 2\n");
	if (strcmp(html_content.form[2].key,"action") != 0)
		os_printf("Erreur clef parametre 3\n");
	if (strcmp(html_content.form[2].value,"Submit") != 0)
		os_printf("Erreur valeur parametre 3\n");
}

#endif
