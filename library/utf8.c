/*
 * utf8.c
 *
 *  Created on: 26 juin 2018
 *      Author: gautier
 */

/* utf8 from %20 to %2f */
#include "esp_common.h"

struct utf8dec {
	int min;
	int max;
	char *charac;
};

char charac20[] = { ' ', '!', '\"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/' };
char charac3A[] = { ':', ';', '<', '=', '>', '?', '@' };
char charac5B[] = { '[', '\\', ']', '^', '_', '`' };
char charac7B[] = { '{', '|', '}', '~' };

struct utf8dec utf_table[] = { { 0x20, 0x2f, charac20 }, { 0x3A, 0x40, charac3A }, { 0x5B, 0x60, charac5B }, { 0x7B, 0x7E, charac7B } };

/* TODO: to be tested deeply */
static char convert_UTF8_ascii(char *str)
{
	char ret;
	int code;
	int i;

	if (str[0] != '%')
		return -1;

	if (str[1] >= '2' && str[1] <= '7') {
		code = (str[1] - '0') * 16;
		if (str[2] >= '0' && str[2] <= '9') {
			code += (str[2] - '0');
		} else if (str[2] >= 'a' && str[2] <= 'f') {
			code += (10 + str[2] - 'a');
		} else if (str[2] >= 'A' && str[2] <= 'F') {
			code += (10 + str[2] - 'A');
		} else {
			return -1;
		}
	} else {
		return -1;
	}
	ret = -1;
	for (i = 0; i < sizeof(utf_table) / sizeof(struct utf8dec); i++) {
		if (code >= utf_table[i].min && code <= utf_table[i].max) {
			ret = utf_table[i].charac[code - utf_table[i].min];
			break;
		}
	}
	return ret;
}

/* TODO: to be tested deeply */
void convert_UTF8_string(char *from)
{
	char *to;
	char a;

	to = from;
	while (*from != 0) {
		if (*from == '%') {
			a = convert_UTF8_ascii(from);
			from += 3;
			*to++ = a;
		} else {
			if (*from == '+') {		// Space can be encoded as '+' or as '%20'. Last case is handled above
				*to++ = ' ';
				from++;
			} else {
				*to++ = *from++;
			}
		}
	}
	*to = 0;
}
