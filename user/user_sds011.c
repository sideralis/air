/*
 * user_sds011.c
 *
 *  Created on: 17 avr. 2018
 *      Author: gautier
 */

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

/* Functions */

// Interrupt on falling edge
static void gpio_interrupt_edge(void *arg)
{
	signed portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	frc2_before = READ_PERI_REG(FRC2_COUNT_ADDRESS);
	GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 1);

	uint32 status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);          //READ STATUS OF INTERRUPT
	static uint8 val = 0;

	if (status & GPIO_Pin_5) {
		xSemaphoreGiveFromISR(semaphore_uart_start, &xHigherPriorityTaskWoken);
		if (xHigherPriorityTaskWoken)
			PendSV(1);
	}

	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, status);       //CLEAR THE STATUS IN THE W1 INTERRUPT REGISTER
	GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 0);
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
			GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 1);
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
			GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 0);
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
int check_and_decode(uint8 *data, uint32 *pm25, uint32 *pm10)
{
	int err;
	uint8 checksum;
	if (data[0] != 0xaa || data[1] != 0xc0 || data[9] != 0xab) {
		return -1;
	}
	checksum = data[2] + data[3] + data[4] + data[5] + data[6] + data[7];
	if (checksum != data[8])
		return -2;
	*pm25 = data[3];
	*pm25 <<= 8;
	*pm25 += data[2];
	*pm10 = data[5];
	*pm10 <<= 8;
	*pm10 += data[4];

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
	uint32 pm25, pm10;
	struct mqtt_msg mqtt_pm;

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
			GPIO_OUTPUT_SET(GPIO_ID_PIN(2), 1);
			bit_val = gpio_input_get();
			bit_val >>= GPIO_ID_PIN(5);
			bit_val &= 0b1;
			state_machine = sm_data(state_machine, bit_val, &sds011[received_data]);
			GPIO_OUTPUT_SET(GPIO_ID_PIN(2), 0);
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
			GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 1);
			os_printf("ERR: Frame error!\n");
			GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 0);
			vTaskDelay(500 / portTICK_RATE_MS);		// Wait 0.5s
		} else {

//			for (i = 0; i < 10; i++) {
//				os_printf("DBG: 0x%x ", sds011[i]/*(frc2_count[i]-frc2_count[i-1])*16/80*/);
//			}
//			os_printf("\n");
			i = check_and_decode(sds011, &pm25, &pm10);
			if (i == -1) {
				GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 1);
				os_printf("ERR: header!\n");
				GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 0);
			} else if (i == -2) {
				GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 1);
				os_printf("ERR: checksum!\n");
				GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 0);
			} else {
				os_printf("INFO: PM2.5 = %d -- PM10 = %d\n", pm25, pm10);
				mqtt_pm.pm10 = pm10;
				mqtt_pm.pm25 = pm25;
				xQueueSend(mqtt_msg_queue, &mqtt_pm, 0);
			}
		}

		/* Re enable GPIO interrupt to get next data */
		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 0xffff);       // Clear all GPIO pending interrupt
		ETS_GPIO_INTR_ENABLE();

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

	uint32 frc2_ctrl;

	frc2_ctrl = READ_PERI_REG(FRC2_CTRL_ADDRESS);
	os_printf("DBG: FRC2 ctrl = 0x%x\n", frc2_ctrl);

	/* Attempt to create a semaphore. */
	vSemaphoreCreateBinary(semaphore_uart_start);
	if (semaphore_uart_start == NULL) {
		os_printf("ERR: Can not create semaphore!\n");
		return;
	}
	// Take semaphore, will be released by interrupt
	ret = xSemaphoreTake(semaphore_uart_start, portMAX_DELAY);

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

	// configure D2 pad to GPIO ouput mode
	io_out_conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	io_out_conf.GPIO_Mode = GPIO_Mode_Output;
	io_out_conf.GPIO_Pin = GPIO_Pin_4;
	io_out_conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&io_out_conf);

	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 0);

	// configure D1 pad to GPIO input mode and get interrupt when falling edge
	io_out_conf.GPIO_IntrType = GPIO_PIN_INTR_NEGEDGE;
	io_out_conf.GPIO_Mode = GPIO_Mode_Input;
	io_out_conf.GPIO_Pin = GPIO_Pin_5;
	io_out_conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&io_out_conf);

	gpio_intr_handler_register(gpio_interrupt_edge, NULL);

	xTaskCreate(task_data_read, "data uart", 256, NULL, 3, NULL);

	/* Enable GPIO interrupt */
	ETS_GPIO_INTR_ENABLE();

	for (;;) {
	}
}
