/*
 * user_led.h
 *
 *  Created on: 21 juin 2018
 *      Author: gautier
 */

#ifndef INCLUDE_USER_LED_H_
#define INCLUDE_USER_LED_H_

struct led_info {
	int state;						/* ON, OFF, BLINKING, ... */		/* FIXME: replace by an enum */
	int color_to;					/* The target color */
	int color_from;					/* The 2nd color target in case of blinking or fading */
};

/* Different state of the led */
#define LED_OFF				0
#define LED_ON				2
#define LED_BLINK			1
#define LED_FADE_IN			3
#define LED_FADE_OUT		4
#define LED_FADE_BLINK		5

/* Intensity of the led */
#define LED_INTENSITY_VERY_BRIGHT	4
#define LED_INTENSITY_BRIGHT		3
#define LED_INTENSITY_DIM			2
#define LED_INTENSITY_VERY_DIM		1

/* Color of the led */
#define LED_RED				0xff0000
#define LED_GREEN			0x00ff00
#define LED_BLUE			0x0000ff

#define LED_YELLOW			0xffff00
#define LED_MAGENTA			0xff00ff
#define LED_CYAN			0x00ffff

#define LED_WHITE			0xffffff
#define LED_BLACK			0x000000

#define LED_VIOLET			0x7f00ff
#define LED_ORANGE			0xff7f00

/* Various macro */
#define GET_RED(a)			((a>>16) & 0xff)
#define GET_GREEN(a)		((a>>8) & 0xff)
#define GET_BLUE(a)			((a>>0) & 0xff)

#endif /* INCLUDE_USER_LED_H_ */
