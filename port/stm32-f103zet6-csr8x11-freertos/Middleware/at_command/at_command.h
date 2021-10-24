/******************************************************************************
  * @file           at_command.h
  * @author         leon
  * @version        V0.1
  * @date           2021-05-16
  * @brief
******************************************************************************/

#ifndef __AT_COMMAND_H__
#define __AT_COMMAND_H__
#include "stdio.h"
#include "stdint.h"
#include "string.h"

#define AT_COMMAND_LOG_INFO printf
#define AT_COMMAND_ASSERT(expr) assert_param(expr)

#define AT_COMMAND_NAME_MAX_LENGTH	20
#define AT_COMMAND_ITEM_MAX_COUNT	20
#define AT_COMMAND_MAX_LENGTH		50

#define AT_COMMAND_HEADER_A	   'A'
#define AT_COMMAND_HEADER_T    'T'
#define AT_COMMAND_HEADER_PLUS '+'
#define AT_COMMAND_EQUAL	   '='	
#define AT_COMMAND_TAIL_R	   0x0D
#define AT_COMMAND_TAIL_N      0x0A

#define AT_COMMAND_HEADER_LENGTH 3 //"AT+"
#define AT_COMMAND_TAIL_LENGTH	 2 //"\r\n"
#define AT_COMMAND_PARSE_TIMEOUT_LENGTH 100

typedef enum {
	AT_COMMAND_EXECUTE_TIMEOUT = -4,
	AT_COMMAND_PARSE_TIMEOUT = -3,
	AT_COMMAND_INVALID_PARAM = -2,
	AT_COMMAND_FAIL = -1,
	AT_COMMAND_OK = 0,
} at_command_status_t;

typedef void (*at_command_handler_t)(void *param, uint8_t param_length);

typedef struct {
	const char *name;
	at_command_handler_t handler;
} at_command_item_t;

#if 0
#define AT_COMMAND_ACTIVE    0
#define AT_COMMAND_EXECUTION 1
typedef uint8_t at_command_mode_t;

typedef enum {
    AT_COMMAND_WAIT_4_HEADER_A,
    AT_COMMAND_WAIT_4_HEADER_T,
	AT_COMMAND_WAIT_4_HEADER_PLUS,
	AT_COMMAND_WAIT_4_NAME,
	AT_COMMAND_WAIT_4_PARAM,
	AT_COMMAND_WAIT_4_TAIL_R,
	AT_COMMAND_WAIT_4_TAIL_N,
} at_command_parse_state_t;
#endif

typedef struct {
	uint8_t *buffer;
	uint8_t at_cmd_tatal_length;
	uint8_t at_cmd_name_length;
	uint8_t at_cmd_param_offset;
	uint8_t at_cmd_param_length;
} at_command_parse_t;

typedef enum {
	AT_COMMAND_STATE_IDLE = 0,
	AT_COMAND_STATE_PARSING,
	AT_COMAND_STATE_PARSE_DONE,
	AT_COMMAND_STATE_EXECUTING,
} at_command_state_t;

#define AT_COMMAND_RESPONSE_MAX_LENGTH 100

typedef struct {
	char buffer[AT_COMMAND_RESPONSE_MAX_LENGTH];
	uint8_t length;
} at_command_response_t;

at_command_status_t at_command_init(void);

at_command_status_t at_command_register_handler(at_command_item_t *item);

at_command_status_t at_command_deregister_handler(at_command_item_t *item);

at_command_status_t at_command_deinit(void);

void vTaskATCommand(void *pvParameters);

void at_command_uart_rx_isr_handler(uint8_t data);

void at_command_send_response(at_command_response_t *response);

#endif //__AT_COMMAND_H__


