/*
 * user_led.c
 *
 *  Created on: 20 juin 2018
 *      Author: gautier
 */

#include "esp_common.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "user_led.h"
#include "user_queue.h"

#define PWM_0_OUT_IO_MUX PERIPHS_IO_MUX_MTDI_U
#define PWM_0_OUT_IO_NUM 12
#define PWM_0_OUT_IO_FUNC  FUNC_GPIO12

#define PWM_2_OUT_IO_MUX PERIPHS_IO_MUX_MTCK_U
#define PWM_2_OUT_IO_NUM 13
#define PWM_2_OUT_IO_FUNC  FUNC_GPIO13

#define PWM_3_OUT_IO_MUX PERIPHS_IO_MUX_MTDO_U
#define PWM_3_OUT_IO_NUM 15
#define PWM_3_OUT_IO_FUNC  FUNC_GPIO15

#define PWM_NUM_CHANNEL_NUM    3  //number of PWM channels/leds

int led_state;
int led_color_to;
int led_color_from;
int led_color_current;
int led_intensity;
int led_time_blink;
int led_speed_fade;

static void led_set_duty_inv(uint32 duty, uint8 channel) {
	duty = 1023-duty;
	pwm_set_duty(duty, channel);
}
static void led_set_duty_nor(uint32 duty, uint8 channel) {
	pwm_set_duty(duty, channel);
}

void task_led(void *param)
{
	int r,g,b;
	int r1,g1,b1;
	int queue_size;
	struct led_info led_setup;
	void (*led_set_duty)(uint32, uint8);

	uint32 io_info[][3] = {
            { PWM_0_OUT_IO_MUX, PWM_0_OUT_IO_FUNC, PWM_0_OUT_IO_NUM }, //Channel 0		RED
            { PWM_2_OUT_IO_MUX, PWM_2_OUT_IO_FUNC, PWM_2_OUT_IO_NUM }, //Channel 1		BLUE
            { PWM_3_OUT_IO_MUX, PWM_3_OUT_IO_FUNC, PWM_3_OUT_IO_NUM }, //Channel 2		GREEN
    };
	u32 duty[3] = {0, 0, 0}; //Max duty cycle is 1023
	pwm_init(1000, duty ,PWM_NUM_CHANNEL_NUM,io_info);

	led_state = LED_OFF;
	led_color_to = LED_ORANGE;
	led_color_from = LED_BLUE;
	led_intensity = LED_INTENSITY_DIM;
	led_time_blink = 500;

	if (*(int *)param == LED_TYPE_RGB)
		led_set_duty = led_set_duty_inv;
	else
		led_set_duty = led_set_duty_nor;

	while(1) {
		switch (led_state) {
		case LED_OFF:
			led_set_duty(0, 0); // RED
			led_set_duty(0, 2); // GREEN
			led_set_duty(0, 1); // BLUE

			led_color_current = 0;

			pwm_start();

			// Wait for next event
			xQueueReceive(led_queue, &led_setup, portMAX_DELAY);
			printf("DBG: Receive led command - %d %d %d\n",led_setup.state,led_setup.color_to, led_setup.color_from );
			led_state = led_setup.state;
			led_color_to = led_setup.color_to;
			led_color_from = led_setup.color_from;
			break;
		case LED_ON:
			r = GET_RED(led_color_to) * led_intensity;
			g = GET_GREEN(led_color_to) * led_intensity;
			b = GET_BLUE(led_color_to) * led_intensity;

			led_set_duty(r, 0); // RED
			led_set_duty(g, 2); // GREEN
			led_set_duty(b, 1); // BLUE

			led_color_current = led_color_to;

			pwm_start();

			// Wait for next event
			xQueueReceive(led_queue, &led_setup, portMAX_DELAY);
			led_state = led_setup.state;
			led_color_to = led_setup.color_to;
			led_color_from = led_setup.color_from;
			break;
		case LED_BLINK:
			r = GET_RED(led_color_to) * led_intensity;
			g = GET_GREEN(led_color_to) * led_intensity;
			b = GET_BLUE(led_color_to) * led_intensity;

			led_set_duty(r, 0); // RED
			led_set_duty(g, 2); // GREEN
			led_set_duty(b, 1); // BLUE
			pwm_start();

			led_color_current = led_color_to;

			vTaskDelay(led_time_blink / portTICK_RATE_MS);				// Blinks

			int tmp = led_color_to;
			led_color_to = led_color_from;
			led_color_from = tmp;

			// Check if we have a new led setup
			queue_size = uxQueueMessagesWaiting(led_queue);
			if (queue_size != 0) {
				xQueueReceive(led_queue, &led_setup, portMAX_DELAY);
				led_state = led_setup.state;
				led_color_to = led_setup.color_to;
				led_color_from = led_setup.color_from;
			}

			break;
		case LED_FADE_IN:
			// TODO: implement FADE IN
			r = GET_RED(led_color_current) * led_intensity;
			g = GET_GREEN(led_color_current) * led_intensity;
			b = GET_BLUE(led_color_current) * led_intensity;
			r1 = GET_RED(led_color_to) * led_intensity;
			g1 = GET_GREEN(led_color_to) * led_intensity;
			b1 = GET_BLUE(led_color_to) * led_intensity;
			break;
		case LED_FADE_OUT:
			// TODO: implement FADE OUT
			break;
		case LED_FADE_BLINK:
			// TODO: implement FADE BLINK
			break;

		}
	}
}
