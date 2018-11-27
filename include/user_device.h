/*
 * user_device.h
 *
 *  Created on: 26 nov. 2018
 *      Author: gautier
 */

#ifndef INCLUDE_USER_DEVICE_H_
#define INCLUDE_USER_DEVICE_H_

struct device {
	char mac[13];
	char token[64];
	unsigned char ssid;
};

extern struct device this_device;

#endif /* INCLUDE_USER_DEVICE_H_ */
