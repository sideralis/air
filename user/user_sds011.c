/*
 * user_sds011.c
 *
 *  Created on: 17 avr. 2018
 *      Author: gautier
 */

//#define TRACE
#include "esp_common.h"
#include "gpio.h"

#include "freertos/semphr.h"

#include "user_mqtt.h"
#include "user_queue.h"

/* Defines */
#define ETS_GPIO_INTR_ENABLE()  _xt_isr_unmask(1 << ETS_GPIO_INUM)  //ENABLE INTERRUPTS
#define ETS_GPIO_INTR_DISABLE() _xt_isr_mask(1 << ETS_GPIO_INUM)    //DISABLE INTERRUPTS

#define UART_START 	0
#define UART_DATA 	1
#define UART_STOP	2
#define UART_ERROR	3

/* Global variables */
xSemaphoreHandle semaphore_uart_start = NULL;
static uint32 frc2_before, frc2_after;

/* Structures */
struct sds011_rx {
	unsigned char type;					// Type of answer 0xc0 is for pm report, 0xc5 is for command report
	union {
		unsigned char raw[6];			// The raw data
		struct sds011_c0 {				// 0xC0 answer (pm measurements)
			unsigned short pm25;
			unsigned short pm10;
			unsigned short id;
		} c0;
		struct sds011_c5_8 {			// Answer to command 8 (set or query working period)
			unsigned char cmd;
			unsigned char mode;
			unsigned char time;
			unsigned char zero;
			unsigned short id;
		} c5_8;
	} u;
};

/* Functions */

// Interrupt on falling edge
static void gpio_interrupt_edge(void *arg)
{
	signed portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	frc2_before = READ_PERI_REG(FRC2_COUNT_ADDRESS);
#ifdef TRACE
	GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 1);
#endif
	uint32 status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);          //READ STATUS OF INTERRUPT
	static uint8 val = 0;

	if (status & GPIO_Pin_5) {
		xSemaphoreGiveFromISR(semaphore_uart_start, &xHigherPriorityTaskWoken);
		if (xHigherPriorityTaskWoken)
			PendSV(1);
	}

	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, status);       //CLEAR THE STATUS IN THE W1 INTERRUPT REGISTER
#ifdef TRACE
	GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 0);
#endif
}
int sm_data(int state, int bit, char *data)
{
	static int bit_idx;
	static int timeout = 0;
	switch (state) {
	case UART_START:
		if (bit == 0) {
			// Correct start bit, next step is to get data
			state = UART_DATA;
			bit_idx = 0;
			*data = 0;
		} else {
			// We are still in idle, start bit should come.
			timeout++;
			if (timeout == 10)
				state = UART_ERROR;		// Too long idle, we go for an error
		}
		break;
	case UART_DATA:
		*data |= (bit << bit_idx);
		bit_idx += 1;
		if (bit_idx == 8) {
			// Last bit received, next step is stop bit
			state = UART_STOP;
		}
		break;
	case UART_STOP:
		if (bit == 1) {
			// Correct stop bit, next step is waiting for start bit
			state = UART_START;
			timeout = 0;
		} else {
			// Incorrect bit, we go for an error
			state = UART_ERROR;
		}
		break;
	}
	return state;
}

int check_and_decode(uint8 *data, uint32 *pm25, uint32 *pm10, struct sds011_rx *data2)
{
	int err;
	uint8 checksum;
	int i;

	if (data[0] != 0xaa || data[9] != 0xab) {
		return -1;
	}
	checksum = data[2] + data[3] + data[4] + data[5] + data[6] + data[7];
	if (checksum != data[8])
		return -2;

	data2->type = data[1];
	for (i = 0; i < 6; i++)
		data2->u.raw[i] = data[i + 2];

	return 0;
}
void task_data_read(void *param)
{
	signed portBASE_TYPE ret;
	uint32 bit_val, bit_idx;
	int i, received_data;
	uint32 delta, target;
	uint32 delay;
	uint8 sds011[10];
	uint8 state_machine;
	struct mqtt_msg mqtt_pm;
	struct sds011_rx data_extracted;

	for (;;) {
		// Wait for interrupt (should be start bit)
		ret = xSemaphoreTake(semaphore_uart_start, portMAX_DELAY);
		ETS_GPIO_INTR_DISABLE();
		vTaskSuspendAll();
//		os_delay_us(52);
		received_data = 0;
		state_machine = UART_START;
		delay = 102 - 16;
		bit_idx = 0;
		do {
#ifdef TRACE
			GPIO_OUTPUT_SET(GPIO_ID_PIN(2), 1);
#endif
			bit_val = gpio_input_get();
			bit_val >>= GPIO_ID_PIN(5);
			bit_val &= 0b1;
			state_machine = sm_data(state_machine, bit_val, &sds011[received_data]);
#ifdef TRACE
			GPIO_OUTPUT_SET(GPIO_ID_PIN(2), 0);
#endif
			if (state_machine == UART_STOP)
				received_data += 1;
			else if (state_machine == UART_ERROR)
				break;
			bit_idx++;
			// Calculate time to wait
			frc2_after = READ_PERI_REG(FRC2_COUNT_ADDRESS);
			if (frc2_after > frc2_before) {
				target = frc2_before * 100 + 20000 + bit_idx * 52083;// 20000 is the initial offset (theoretically it should be half of a bit duration) but it is less due to time between interrupt and this point
				delta = target - frc2_after * 100;
				delay = delta << 4;
				delay /= 8000;
			}
			os_delay_us(delay);
		} while (received_data < 10);
		xTaskResumeAll();
		if (state_machine == UART_ERROR) {
#ifdef TRACE
			GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 1);
#endif
			os_printf("ERR: Frame error!\n");
#ifdef TRACE
			GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 0);
#endif
			vTaskDelay(500 / portTICK_RATE_MS);		// Wait 0.5s
		} else {
			i = check_and_decode(sds011, &data_extracted);
			if (i == -1) {
#ifdef TRACE
				GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 1);
#endif
				os_printf("ERR: header!\n");
#ifdef TRACE
				GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 0);
#endif
			} else if (i == -2) {
#ifdef TRACE
				GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 1);
#endif
				os_printf("ERR: checksum!\n");
#ifdef TRACE
				GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 0);
#endif
			} else {
				if (data_extracted.type == 0xc0) {
					os_printf("INFO: PM2.5 = %d -- PM10 = %d\n", data_extracted.u.c0.pm25, data_extracted.u.c0.pm10);
					mqtt_pm.pm10 = data_extracted.u.c0.pm10;
					mqtt_pm.pm25 = data_extracted.u.c0.pm25;
					xQueueSend(mqtt_msg_queue, &mqtt_pm, 0);
				} else {
					os_printf("INFO: Answer to cmd %d, id=%d mode=%d time=%d mn\n", data_extracted.u.c5_8.cmd, data_extracted.u.c5_8.id,
							data_extracted.u.c5_8.mode, data_extracted.u.c5_8.time);
				}
			}
		}

		/* Re enable GPIO interrupt to get next data */
		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 0xffff);       // Clear all GPIO pending interrupt
		ETS_GPIO_INTR_ENABLE();

	}

}
void send_byte(char data)
{
	int i;
	char bit;

	vTaskSuspendAll();
	// Send start byte as 0
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 0);
	os_delay_us(103);

	// Send byte from LSB to MSB
	for (i = 0; i < 8; i++) {
		bit = data & 0b01;
		data = data >> 1;
		GPIO_OUTPUT_SET(GPIO_ID_PIN(4), bit);
		os_delay_us(103);

	}
	// Send stop byte as 1
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 1);
	os_delay_us(103);

	xTaskResumeAll();

}
void sds011_data_write(char *data, int nb_data)
{
	int i;
	unsigned char crc = 0;

	for (i = 0; i < nb_data; i++) {
		if (i >= 2 && i < nb_data - 2) {
			crc += data[i];
			send_byte(data[i]);
		} else if (i == nb_data - 2) {
			crc &= 0xff;
			send_byte(crc);
		} else {
			send_byte(data[i]);
		}
	}
}

// ===================
// Connection
// 5v = VV (on Lolin v3 else VIN)
// GND = GND
// RX = D2
// TX = D1
void task_sds011(void *param)
{
	signed portBASE_TYPE ret;
	GPIO_ConfigTypeDef io_out_conf;

	/* Attempt to create a semaphore. */
	vSemaphoreCreateBinary(semaphore_uart_start);
	if (semaphore_uart_start == NULL) {
		os_printf("ERR: Can not create semaphore!\n");
		return;
	}
	// Take semaphore, will be released by interrupt
	ret = xSemaphoreTake(semaphore_uart_start, portMAX_DELAY);

#ifdef TRACE
	// configure D5 pad to GPIO ouput mode			// For DBG only!
	io_out_conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	io_out_conf.GPIO_Mode = GPIO_Mode_Output;
	io_out_conf.GPIO_Pin = GPIO_Pin_14;
	io_out_conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&io_out_conf);

	GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 0);

	// configure D4 pad to GPIO ouput mode			// For DBG only!
	io_out_conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	io_out_conf.GPIO_Mode = GPIO_Mode_Output;
	io_out_conf.GPIO_Pin = GPIO_Pin_2;
	io_out_conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&io_out_conf);

	GPIO_OUTPUT_SET(GPIO_ID_PIN(2), 0);

	// configure D3 pad to GPIO ouput mode			// For DBG only!
	io_out_conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	io_out_conf.GPIO_Mode = GPIO_Mode_Output;
	io_out_conf.GPIO_Pin = GPIO_Pin_0;
	io_out_conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&io_out_conf);

	GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 0);
#endif

	// configure D2 pad to GPIO ouput mode					 						= TX
	io_out_conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	io_out_conf.GPIO_Mode = GPIO_Mode_Output;
	io_out_conf.GPIO_Pin = GPIO_Pin_4;
	io_out_conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&io_out_conf);

	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 1);

	// configure D1 pad to GPIO input mode and get interrupt when falling edge		= RX
	io_out_conf.GPIO_IntrType = GPIO_PIN_INTR_NEGEDGE;
	io_out_conf.GPIO_Mode = GPIO_Mode_Input;
	io_out_conf.GPIO_Pin = GPIO_Pin_5;
	io_out_conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&io_out_conf);

	gpio_intr_handler_register(gpio_interrupt_edge, NULL);

	xTaskCreate(task_data_read, "data uart", 256, NULL, 3, NULL);

	vTaskDelay(500 / portTICK_RATE_MS);		// Wait 0.5s before emiting anything

	char sds011_working_period[] = { 0xaa, 0xb4, 8, 0x01, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0x00, 0xab };

	/* Send command */
	os_printf("INFO: Sending commands working period to sds011\n");
	sds011_data_write(sds011_working_period, sizeof(sds011_working_period));

	/* Enable GPIO interrupt */
	ETS_GPIO_INTR_ENABLE();

	for (;;) {
	}
}
