/*
 * user_html.h
 *
 *  Created on: 28 juin 2018
 *      Author: gautier
 */

#ifndef INCLUDE_USER_HTML_H_
#define INCLUDE_USER_HTML_H_

#define METHOD_GET 		0b01
#define METHOD_POST 	0b10
#define METHOD_ALL		0b11

#define AIR_SERVER_IP 	{95, 123, 99, 121}			// This is the ip address of our server.

#define MAX_LENGHT_USER_ID	64						// Max length for storing email address of user

// The parameters given with a page (found in the page name for a GET and in content for a POST)
struct page_param {
	char key[16];
	char value[48];
};

// A description of the page received
struct header_html_recv {
	char page_name[64];				// name of the page
	short method;					// method (GET/POST)
	struct page_param form[10];		// The parameters associated with the page
};

int process_header_recv(char *, struct header_html_recv *);
char *display_network_choice(void);
char *display_network_choosen(void);
char *display_404(void);
char *html_add_header(char *);

#endif /* INCLUDE_USER_HTML_H_ */
