/*
 * user_wifi.h
 *
 *  Created on: 22 juin 2018
 *      Author: gautier
 */

#ifndef INCLUDE_USER_WIFI_H_
#define INCLUDE_USER_WIFI_H_

/* Structure used to store info on wifi station around */
struct router_info {
    SLIST_ENTRY(router_info)     next;

    u8  bssid[6];
    u8  ssid[33];
    u8  channel;
    u8  authmode;

    u16 rx_seq;
    u8  encrytion_mode;
    u8  iv[8];
    u8  iv_check;
    u8	rssi;
};		// TODO clean structure, some fields are unused

/* Status used to indicate if we continue scanning or if we stop */
#define WIFI_DETECTED 			1
#define WIFI_NOT_DETECTED		0

/* Prototypes */
void wifi_handle_event_cb(System_Event_t *);

#endif /* INCLUDE_USER_WIFI_H_ */
