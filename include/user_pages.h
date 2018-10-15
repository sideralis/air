/*
 * user_pages.h
 *
 *  Created on: 11 oct. 2018
 *      Author: gautier
 */

#ifndef INCLUDE_USER_PAGES_H_
#define INCLUDE_USER_PAGES_H_

int page_wifi(struct header_html_recv *, struct espconn *);
int page_connect(struct header_html_recv *, struct espconn *);

#endif /* INCLUDE_USER_PAGES_H_ */
