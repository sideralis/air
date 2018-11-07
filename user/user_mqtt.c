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

#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mqtt/MQTTClient.h"

#include "user_mqtt.h"
#include "ssl_server_crt.h"

ssl_ca_crt_key_t ssl_cck;

#define SSL_CA_CERT_KEY_INIT(s,a,b,c,d,e,f)  ((ssl_ca_crt_key_t *)s)->cacrt = a;\
                                             ((ssl_ca_crt_key_t *)s)->cacrt_len = b;\
                                             ((ssl_ca_crt_key_t *)s)->cert = c;\
                                             ((ssl_ca_crt_key_t *)s)->cert_len = d;\
                                             ((ssl_ca_crt_key_t *)s)->key = e;\
                                             ((ssl_ca_crt_key_t *)s)->key_len = f;

LOCAL xTaskHandle mqttc_client_handle;

static void messageArrived(MessageData* data)
{
	printf("Message arrived: %s\n", data->message->payload);
}
static void mqtt_client_thread(void* pvParameters)
{
	vTaskDelay(5000 / portTICK_RATE_MS);  			// wait every 5 seconds before we start

	MQTTClient client;
	Network network;
	unsigned char sendbuf[80], readbuf[80] = { 0 };
	int rc = 0, count = 10;

	MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

	pvParameters = 0;
	NetworkInitSSL(&network);

	MQTTClientInit(&client, &network, 30000, sendbuf, sizeof(sendbuf), readbuf, sizeof(readbuf));

	char* address = MQTT_BROKER;

	SSL_CA_CERT_KEY_INIT(&ssl_cck, ca_crt, ca_crt_len, NULL, 0, NULL, 0);

	if ((rc = NetworkConnectSSL(&network, address, MQTT_PORT, &ssl_cck, TLSv1_2_client_method(), SSL_VERIFY_PEER, 8192)) != 0) {
		printf("Return code from network connect is %d\n", rc);
	}
	os_printf("DBG: mqtt network connect done\n");

#if defined(MQTT_TASK)

	if ((rc = MQTTStartTask(&client)) != pdPASS) {
		printf("Return code from start tasks is %d\n", rc);
	} else {
		printf("Use MQTTStartTask\n");
	}

#endif

	connectData.MQTTVersion = 4;
	connectData.clientID.cstring = CLIENT_ID;
	connectData.username.cstring = "use-token-auth";
	connectData.password.cstring = TOKEN;

	if ((rc = MQTTConnect(&client, &connectData)) != 0) {
		os_printf("Return code from MQTT connect is %d\n", rc);
	} else {
		os_printf("DBG: mqtt connect done\n");
	}

	while (--count) {
		MQTTMessage message;
		char payload[30];

		message.qos = QOS2;
		message.retained = 0;
		message.payload = payload;
		sprintf(payload, "{\"hello Bernard\": %d}", count);
		message.payloadlen = strlen(payload);

		if ((rc = MQTTPublish(&client, "iot-2/evt/status/fmt/json", &message)) != 0) {
			os_printf("Return code from MQTT publish is %d\n", rc);
		} else {
			os_printf("MQTT publish topic \"iot-2/evt/status/fmt/json\", message is %s\n", payload);
		}

		vTaskDelay(1000 / portTICK_RATE_MS);  //send every 1 seconds
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
