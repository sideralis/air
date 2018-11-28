/*
 * user_mqtt.h
 *
 *  Created on: 7 nov. 2018
 *      Author: gautier
 */

#ifndef INCLUDE_USER_MQTT_H_
#define INCLUDE_USER_MQTT_H_

#define MQTT_BROKER  "penwv1.messaging.internetofthings.ibmcloud.com"  /* MQTT Broker Address*/
#define MQTT_PORT    8883             /* MQTT Port*/

#define MQTT_CLIENT_THREAD_NAME         "mqtt_client_thread"
#define MQTT_CLIENT_THREAD_STACK_WORDS  2048
#define MQTT_CLIENT_THREAD_PRIO         8

#define CLIENT_ID		"d:penwv1:air:%s"

struct mqtt_msg {
	uint32 pm25;
	uint32 pm10;
};

#endif /* INCLUDE_USER_MQTT_H_ */
