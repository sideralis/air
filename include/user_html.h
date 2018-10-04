/*
 * user_html.h
 *
 *  Created on: 28 juin 2018
 *      Author: gautier
 */

#ifndef INCLUDE_USER_HTML_H_
#define INCLUDE_USER_HTML_H_

#define METHOD_GET 1
#define METHOD_POST 2

struct page_param {
	char key[16];
	char value[48];
};

struct header_html_recv {
	char page_name[32];
	short method;
	struct html_param form[10];
};

int process_header_recv(char *, struct header_html_recv *);
char *display_network_choice(void);
char *display_network_choosen(void);
char *display_404(void);
char *html_add_header(char *);

#endif /* INCLUDE_USER_HTML_H_ */
