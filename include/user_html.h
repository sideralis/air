/*
 * user_html.h
 *
 *  Created on: 28 juin 2018
 *      Author: gautier
 */

#ifndef INCLUDE_USER_HTML_H_
#define INCLUDE_USER_HTML_H_

struct header_html_recv {
	short get;
	short push;
	char page[32];
};

int process_header_recv(char *, struct header_html_recv *);
char *display_network_choice(void);
char *display_network_choosen(void);
char *display_404(void);

#endif /* INCLUDE_USER_HTML_H_ */
