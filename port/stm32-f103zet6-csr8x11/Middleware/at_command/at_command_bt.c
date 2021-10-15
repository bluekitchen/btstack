/******************************************************************************
  * @file           at_command_bt.c
  * @author         leon
  * @version        V0.1
  * @date           2021-05-16
  * @brief
******************************************************************************/
#include "at_command_bt.h"
#include "at_command.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"
#include "bt_os_layer_api.h"
#include "bsp_uart_fifo.h"
#include "bt_system.h"
#include "bt_gap_classic.h"

void at_command_bt_system_it_handler(void *param, uint8_t param_length)
{
	at_command_response_t response = {0};

	if (param_length == 0 || param == NULL) {
		return;
	}
	if (0 == strncmp((char *)param, "POWER_ON", param_length)) {
		bt_power_on();
		strcpy(response.buffer, "OK");
	} else if (0 == strncmp((char *)param, "POWER_OFF", param_length)) {
		bt_power_off();
		strcpy(response.buffer, "OK");
	} else if (0 == strncmp((char *)param, "INQ", param_length)) {
		bt_gap_inquiry(0x009E8B33, 10, 20);
		strcpy(response.buffer, "OK");
	} else {
		strcpy(response.buffer, "Error, Unknown AT command");
	}
	at_command_send_response(&response);
}

