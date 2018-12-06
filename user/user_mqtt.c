/*
 * user_mqtt.c
 *
 *  Created on: 23 oct. 2018
 *      Author: gautier
 */

/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mqtt/MQTTClient.h"

#include "user_mqtt.h"
#include "user_queue.h"
#include "user_device.h"

#include "ssl_server_crt.h"

#include "lwip/netdb.h"
#include "lwip/netif.h"
#include "lwip/apps/sntp_time.h"
#include "openssl/ssl.h"
#include "MQTTFreeRTOS.h"

ssl_ca_crt_key_t ssl_cck;

#define SSL_CA_CERT_KEY_INIT(s,a,b,c,d,e,f)  ((ssl_ca_crt_key_t *)s)->cacrt = a;\
                                             ((ssl_ca_crt_key_t *)s)->cacrt_len = b;\
                                             ((ssl_ca_crt_key_t *)s)->cert = c;\
                                             ((ssl_ca_crt_key_t *)s)->cert_len = d;\
                                             ((ssl_ca_crt_key_t *)s)->key = e;\
                                             ((ssl_ca_crt_key_t *)s)->key_len = f;

#define TOPIC_PM 		"iot-2/evt/pm/fmt/json"						// TODO define as const to save space ?
#define TOPIC_STATE		"iot-2/evt/etat/fmt/json"

static xTaskHandle mqttc_client_handle;

static void messageArrived(MessageData* data)
{
	printf("Message arrived: %s\n", data->message->payload);
}

static void mqtt_client_thread(void* pvParameters)
{
	MQTTClient client;
	Network network;
	int rc = 0;
	char* address = MQTT_BROKER;
	char clientID[32];						// FIXME check size if big enough
	int time;								// TODO time should be real NTP time

	MQTTMessage message;

	char payload[48];						// FIXME check we don't write too much
	char topic_pm[64];
	char topic_st[64];
	unsigned char sendbuf[80], readbuf[80] = { 0 };

	int heap_size;

	// Do not start as long as we are not connected to internet
	while (wifi_station_get_connect_status() != STATION_GOT_IP) {
		vTaskDelay(5000 / portTICK_RATE_MS);  			// wait every 5 seconds before we start
	}
	os_printf("INFO: MQTT task start as we are connected\n");

	heap_size = system_get_free_heap_size();
	os_printf("DBG: Heap size (mqtt) = %d\n", heap_size);
	if (heap_size < 35000) {
		os_printf("WARNING: heap size is low!");
	}

	MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

	pvParameters = 0;
	NetworkInitSSL(&network);

	MQTTClientInit(&client, &network, 30000, sendbuf, sizeof(sendbuf), readbuf, sizeof(readbuf));

	SSL_CA_CERT_KEY_INIT(&ssl_cck, ca_crt, ca_crt_len, NULL, 0, NULL, 0);

	if ((rc = NetworkConnectSSL(&network, address, MQTT_PORT, &ssl_cck, TLSv1_2_client_method(), SSL_VERIFY_PEER, 4096)) != 0) {
		os_printf("ERR: Return code from NetworkConnectSSL is %d\n", rc);
	} else {
		os_printf("DBG: mqtt network connect done\n");
	}

#if defined(MQTT_TASK)

	if ((rc = MQTTStartTask(&client)) != pdPASS) {
		printf("ERR: Return code from start tasks is %d\n", rc);
	} else {
		printf("DBG: Use MQTTStartTask\n");
	}

#endif
	// Generate topic for state
	if (strlen(TOPIC_STATE) > sizeof(topic_st) - 1) {
		os_printf("ERR: topic table (state) is too small!\n");
		return;
	}
	strcpy(topic_st, TOPIC_STATE);
	sprintf(clientID, CLIENT_ID, this_device.mac);

	connectData.MQTTVersion = 4;
	connectData.clientID.cstring = clientID;
	connectData.username.cstring = "use-token-auth";
	connectData.password.cstring = this_device.token;
	connectData.keepAliveInterval = 600;				// FIXME should depend on sds011 configuration and should not be needed with MQTT_TASK. 120 is not enough in case we have a frame error.
	// LastWill
//	connectData.willFlag = 1;
//	connectData.will.qos = QOS2;
//	connectData.will.retained = 1;
//	connectData.will.message.cstring = "{\"etat\": \"offline\"}";
//	connectData.will.topicName.cstring = topic_st;

	if ((rc = MQTTConnect(&client, &connectData)) != 0) {
		os_printf("ERR: Return code from MQTTConnect is %d\n", rc);
	} else {
		os_printf("DBG: mqtt connect done\n");
	}

	// Send status message (online with lastwill as offline)
	message.qos = QOS2;
	message.retained = 1;
	message.payload = payload;
	sprintf(payload, "{\"etat\": \"online\"}");
	message.payloadlen = strlen(payload);

	if ((rc = MQTTPublish(&client, topic_st, &message)) != 0) {
		os_printf("Return code from MQTT publish is %d\n", rc);
	}
	os_printf("INFO: MQTT publish topic \"%s\", message is %s\n", topic_st, payload);

	if (strlen(TOPIC_PM) > sizeof(topic_pm) - 1) {
		os_printf("ERR: topic table (pm) is too small!\n");
		return;
	}
	strcpy(topic_pm, TOPIC_PM);
	while (1) {
		struct mqtt_msg mqtt_pm;

		// Wait for a new message
		xQueueReceive(mqtt_msg_queue, &mqtt_pm, portMAX_DELAY);

		struct timeval t;
		gettimeofday(&t, NULL);
		time = t.tv_sec/SECSPERMIN - 25733802;				// FIXME we should check that time is set

		message.qos = QOS2;
		message.retained = 0;
		message.payload = payload;
		sprintf(payload, "{\"id\":%d,\"pm25\":%d,\"pm10\":%d}", time, mqtt_pm.pm25, mqtt_pm.pm10);
		message.payloadlen = strlen(payload);

		if ((rc = MQTTPublish(&client, topic_pm, &message)) != 0) {
			os_printf("Return code from MQTT publish is %d\n", rc);
			// FIXME if it does not work it may mean that we have to reconnect.
		}
		os_printf("MQTT publish topic \"iot-2/evt/pm/fmt/json\", message is %s\n", payload);

//		vTaskDelay(1000 / portTICK_RATE_MS);  //send every 1 seconds
	}

	printf("mqtt_client_thread going to be deleted\n");
	vTaskDelete(NULL);
	return;
}

void user_mqtt_init(void)
{
	int ret;

	ret = xTaskCreate(mqtt_client_thread, MQTT_CLIENT_THREAD_NAME, MQTT_CLIENT_THREAD_STACK_WORDS, NULL, MQTT_CLIENT_THREAD_PRIO, &mqttc_client_handle);

	if (ret != pdPASS) {
		os_printf("mqtt create client thread %s failed\n", MQTT_CLIENT_THREAD_NAME);
	} else {
		os_printf("mqtt create client thread %s succeeded\n", MQTT_CLIENT_THREAD_NAME);
	}
}
