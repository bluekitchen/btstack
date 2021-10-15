/******************************************************************************
  * @file           at_command.c
  * @author         leon
  * @version        V0.1
  * @date           2021-05-16
  * @brief
******************************************************************************/
#include "at_command.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"
#include "bt_os_layer_api.h"
#include "bsp_uart_fifo.h"
#include "at_command_bt.h"
static at_command_state_t g_at_command_state = AT_COMMAND_STATE_IDLE;
static TimerHandle_t at_command_timer_handle = NULL;
static at_command_parse_t g_parse_context = {0};
static uint32_t at_command_semaphore = 0;
static bool is_timer_active = false;
static uint8_t buffer[AT_COMMAND_MAX_LENGTH] = {0};
static uint8_t buffer_index = 0;

static at_command_item_t at_cmd_item_table[AT_COMMAND_ITEM_MAX_COUNT] = {
    {"AT+BTSYSIT", at_command_bt_system_it_handler},
};

static void at_command_timeout_callback(TimerHandle_t handle);
static at_command_status_t at_command_timer_stop(void);
static at_command_status_t at_command_parsing(uint8_t *buffer, uint16_t buffer_length, at_command_parse_t *parse_struct);

static at_command_status_t at_command_timer_stop()
{
    if (!is_timer_active) {
        return AT_COMMAND_FAIL;
    }
    if (pdFALSE == xTimerStop(at_command_timer_handle, 0)) {
        return AT_COMMAND_FAIL;
    }
    is_timer_active = false;
    return AT_COMMAND_OK;
}

static at_command_status_t at_command_timer_start(uint32_t timer_length)
{
    uint32_t time = timer_length / portTICK_PERIOD_MS;
    /*if (xTimerIsTimerActive(at_command_timer_handle) != pdFALSE) {
        return AT_COMMAND_FAIL;
    }*/
    if (is_timer_active) {
        return AT_COMMAND_FAIL;
    }
    if (pdFALSE == xTimerChangePeriodFromISR(at_command_timer_handle, time, 0)) {
        return AT_COMMAND_FAIL;
    }
    if (pdFALSE == xTimerResetFromISR(at_command_timer_handle, 0)) {
        return AT_COMMAND_FAIL;
    }
    is_timer_active = true;
    return AT_COMMAND_OK;
}

at_command_status_t at_command_init(void)
{
    AT_COMMAND_LOG_INFO("[AT_COMMAND] at_command_init\r\n");

    if (0 == at_command_semaphore) {
        at_command_semaphore = (uint32_t)xSemaphoreCreateBinary();
        if (0 == at_command_semaphore) {
            return AT_COMMAND_FAIL;
        }
    }
    if (at_command_timer_handle == NULL) {
        at_command_timer_handle = xTimerCreate("AT_CMD_Timer", 0xffff, pdFALSE, NULL, at_command_timeout_callback);
        if (at_command_timer_handle == NULL) {
            return AT_COMMAND_FAIL;
        }
    }
    return AT_COMMAND_OK;
}

at_command_status_t at_command_deinit(void)
{
    AT_COMMAND_LOG_INFO("[AT_COMMAND] at_command_deinit\r\n");

    if (at_command_semaphore) {
        vSemaphoreDelete((SemaphoreHandle_t)at_command_semaphore);
        at_command_semaphore = 0;
    }
    if (at_command_timer_handle) {
        xTimerDelete(at_command_timer_handle, 0);
        at_command_timer_handle = NULL;
    }
    g_at_command_state = AT_COMMAND_STATE_IDLE;

    return AT_COMMAND_OK;
}

at_command_status_t at_command_register_handler(at_command_item_t *item)
{
    uint8_t i = 0;
    if (NULL == item || NULL == item->name || NULL == item->handler) {
        return AT_COMMAND_INVALID_PARAM;
    }

    for (i = 0; i < AT_COMMAND_ITEM_MAX_COUNT; i++) {
        if (at_cmd_item_table[i].handler == NULL) {
            at_cmd_item_table[i].handler = item->handler;
            at_cmd_item_table[i].name = item->name;
            return AT_COMMAND_OK;
        }
    }
    return AT_COMMAND_FAIL;
}

at_command_status_t at_command_deregister_handler(at_command_item_t *item)
{
    uint8_t i = 0;
    if (NULL == item || NULL == item->name || NULL == item->handler) {
        return AT_COMMAND_INVALID_PARAM;
    }
    for (i = 0; i < AT_COMMAND_ITEM_MAX_COUNT; i++) {
        if (at_cmd_item_table[i].handler == item->handler &&
                0 == strncmp((const char *)at_cmd_item_table[i].name, item->name, strlen(item->name))) {
            at_cmd_item_table[i].handler = NULL;
            at_cmd_item_table[i].name = NULL;
            return AT_COMMAND_OK;
        }
    }
    return AT_COMMAND_FAIL;
}


void vTaskATCommand(void *pvParameters)
{
    uint8_t i = 0;
    uint16_t read_length = 0;
    uint16_t rx_fifo_available_length = 0;
    at_command_response_t response = {0};
    while (1) {
        xSemaphoreTake((SemaphoreHandle_t)at_command_semaphore, portMAX_DELAY);
        rx_fifo_available_length = comGetRxFifoAvailableBufferLength(COM1);
        if (rx_fifo_available_length > 0) {
            if (g_parse_context.buffer == NULL) {
                g_parse_context.buffer = (uint8_t *)pvPortMalloc(rx_fifo_available_length);
                AT_COMMAND_ASSERT(g_parse_context.buffer != NULL);
            }
            read_length = comGetBuf(COM1, g_parse_context.buffer, rx_fifo_available_length);
            AT_COMMAND_ASSERT(read_length == rx_fifo_available_length);
            AT_COMMAND_LOG_INFO("[AT_COMMAND] %s", g_parse_context.buffer);
            comClearRxFifo(COM1);
            if (AT_COMMAND_OK == at_command_parsing(g_parse_context.buffer, rx_fifo_available_length, &g_parse_context)) {
                at_command_timer_stop();
                g_at_command_state = AT_COMAND_STATE_PARSE_DONE;
                for (i = 0; i < AT_COMMAND_ITEM_MAX_COUNT; i++) {
                    if ((0 == strncmp((char *)g_parse_context.buffer, at_cmd_item_table[i].name, g_parse_context.at_cmd_name_length)) &&
                            (NULL != at_cmd_item_table[i].handler)) {
                        g_at_command_state = AT_COMMAND_STATE_EXECUTING;
                        at_cmd_item_table[i].handler((void *)(g_parse_context.buffer + g_parse_context.at_cmd_param_offset), g_parse_context.at_cmd_param_length);
                        break;
                    }
                }
            } else {
                at_command_timer_stop();
                strcpy(response.buffer, "Error, Unknown AT command");
                at_command_send_response(&response);
            }
            if (g_parse_context.buffer) {
                memset(g_parse_context.buffer, 0, rx_fifo_available_length);
                vPortFree(g_parse_context.buffer);
            }
            memset((void *)&g_parse_context, 0, sizeof(at_command_parse_t));
            g_at_command_state = AT_COMMAND_STATE_IDLE;
        }
    }
}

void at_command_send_response(at_command_response_t *response)
{
    if (response == NULL) {
        return;
    }
    AT_COMMAND_LOG_INFO("[AT_COMMAND] %s\r\n", response->buffer);
}

void at_command_uart_rx_isr_handler(uint8_t data)
{
    BaseType_t pxHigherPriorityTaskWoken;

    at_command_timer_start(AT_COMMAND_PARSE_TIMEOUT_LENGTH);
    buffer[buffer_index++] = data;
    if (buffer_index > sizeof(buffer)) {
        memset(buffer, 0, sizeof(buffer));
        buffer_index = 0;
        comClearRxFifo(COM1);
        g_at_command_state = AT_COMMAND_STATE_IDLE;
        return;
    }
    g_at_command_state = AT_COMAND_STATE_PARSING;
    if ((buffer_index > AT_COMMAND_TAIL_LENGTH) &&
            (buffer[buffer_index - 2] == AT_COMMAND_TAIL_R) &&
            (buffer[buffer_index - 1] == AT_COMMAND_TAIL_N)) {
        if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED || 0 == at_command_semaphore) {
            return;
        }
        xSemaphoreGiveFromISR((SemaphoreHandle_t)at_command_semaphore, &pxHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
        memset(buffer, 0, sizeof(buffer));
        buffer_index = 0;
    }
}

static void at_command_timeout_callback(TimerHandle_t handle)
{
    if (handle != NULL && handle == at_command_timer_handle) {
        if (AT_COMAND_STATE_PARSING == g_at_command_state) {
            memset((void *)&g_parse_context, 0, sizeof(at_command_parse_t));
            comClearRxFifo(COM1);
            g_at_command_state = AT_COMMAND_STATE_IDLE;
            memset(buffer, 0, AT_COMMAND_MAX_LENGTH);
            buffer_index = 0;
            is_timer_active = false;
            AT_COMMAND_LOG_INFO("[AT_COMMAND] error\r\n");
        } else if (AT_COMMAND_STATE_EXECUTING == g_at_command_state) {
            AT_COMMAND_LOG_INFO("[AT_COMMAND] execute timeout\r\n");
        }
    }
}

static at_command_status_t at_command_parsing(uint8_t *buffer, uint16_t buffer_length, at_command_parse_t *parse_struct)
{
    uint16_t i = 0;
    if (buffer == NULL || parse_struct == NULL || buffer_length == 0) {
        return AT_COMMAND_FAIL;
    }
    if (buffer_length <= (AT_COMMAND_HEADER_LENGTH + AT_COMMAND_TAIL_LENGTH)) { //AT+T\r\n
        return AT_COMMAND_FAIL;
    }
    if (!(buffer[0] == AT_COMMAND_HEADER_A && buffer[1] == AT_COMMAND_HEADER_T && buffer[2] == AT_COMMAND_HEADER_PLUS)) {
        return AT_COMMAND_FAIL;
    }
    if (!(buffer[buffer_length - 2] == AT_COMMAND_TAIL_R && buffer[buffer_length - 1] == AT_COMMAND_TAIL_N)) {
        return AT_COMMAND_FAIL;
    }
    parse_struct->at_cmd_tatal_length = buffer_length;
    for (i = 0; i < parse_struct->at_cmd_tatal_length; i++) {
        if (buffer[i] == AT_COMMAND_EQUAL) {
            parse_struct->at_cmd_name_length = i;
            parse_struct->at_cmd_param_offset = i + 1;
            parse_struct->at_cmd_param_length = parse_struct->at_cmd_tatal_length - parse_struct->at_cmd_param_offset - AT_COMMAND_TAIL_LENGTH;
            break;
        }
    }
    if (i == parse_struct->at_cmd_tatal_length) {
        parse_struct->at_cmd_name_length = parse_struct->at_cmd_tatal_length;
        parse_struct->at_cmd_param_offset = 0;
        parse_struct->at_cmd_param_length = 0;
    }
    return AT_COMMAND_OK;
}

#if 0
static void at_command_parsing(uint8_t data)
{
    if (g_parse_context.buffer_index > AT_COMMAND_MAX_LENGTH) {
        memset((void *)&g_parse_context, 0, sizeof(at_command_parse_t));
        comClearRxFifo(COM1);
        g_at_command_state = AT_COMMAND_STATE_IDLE;
        return;
    }

    switch (g_parse_context.parse_state) {
        case AT_COMMAND_WAIT_4_HEADER_A: {  //'A'
            if (data == AT_COMMAND_HEADER_A) {
                at_command_timer_start(AT_COMMAND_PARSE_TIMEOUT_LENGTH);
                g_at_command_state = AT_COMAND_STATE_PARSING;
                g_parse_context.buffer[g_parse_context.buffer_index++] = data;
                g_parse_context.parse_state = AT_COMMAND_WAIT_4_HEADER_T;
            }
            break;
        }
        case AT_COMMAND_WAIT_4_HEADER_T: { //'T'
            if (data == AT_COMMAND_HEADER_T) {
                g_parse_context.buffer[g_parse_context.buffer_index++] = data;
                g_parse_context.parse_state = AT_COMMAND_WAIT_4_HEADER_PLUS;
            }
            break;
        }
        case AT_COMMAND_WAIT_4_HEADER_PLUS: { //'+'
            if (data == AT_COMMAND_HEADER_PLUS) {
                g_parse_context.buffer[g_parse_context.buffer_index++] = data;
                g_parse_context.parse_state = AT_COMMAND_WAIT_4_NAME;
            }
            break;
        }
        case AT_COMMAND_WAIT_4_NAME: {
            g_parse_context.buffer[g_parse_context.buffer_index++] = data;
            if (data == AT_COMMAND_EQUAL) { //AT+BTPW=1\r\n
                g_parse_context.parse_state = AT_COMMAND_WAIT_4_PARAM;
                g_parse_context.at_cmd_name_length = g_parse_context.buffer_index - 1;
                g_parse_context.at_cmd_param_offset = g_parse_context.buffer_index;
                g_parse_context.mode = AT_COMMAND_EXECUTION;
            } else if (data == AT_COMMAND_TAIL_R) { //AT+PWF\r\n
                g_parse_context.parse_state = AT_COMMAND_WAIT_4_TAIL_N;
                g_parse_context.at_cmd_name_length = g_parse_context.buffer_index - 1;
                g_parse_context.mode = AT_COMMAND_ACTIVE;
            }
            break;
        }
        case AT_COMMAND_WAIT_4_PARAM: {
            g_parse_context.buffer[g_parse_context.buffer_index++] = data;
            if (data == AT_COMMAND_TAIL_R) {
                g_parse_context.parse_state = AT_COMMAND_WAIT_4_TAIL_N;
            }
            break;
        }
        case AT_COMMAND_WAIT_4_TAIL_R: { //\r
            if (data == AT_COMMAND_TAIL_R) {
                g_parse_context.buffer[g_parse_context.buffer_index++] = data;
                g_parse_context.parse_state = AT_COMMAND_WAIT_4_TAIL_N;
            }
            break;
        }
        case AT_COMMAND_WAIT_4_TAIL_N: { //Ω‚ŒˆÕÍ≥…,clear context
            if (data == AT_COMMAND_TAIL_N) {
                g_parse_context.buffer[g_parse_context.buffer_index++] = data;
                g_parse_context.at_cmd_tatal_length = g_parse_context.buffer_index;
                if (AT_COMMAND_EXECUTION == g_parse_context.mode) {
                    g_parse_context.at_cmd_param_length = g_parse_context.buffer_index - g_parse_context.at_cmd_param_offset - AT_COMMAND_TAIL_LENGTH;
                }
                at_command_timer_stop();
                g_parse_context.parse_state = AT_COMMAND_WAIT_4_HEADER_A;
                g_parse_context.buffer_index = 0;
                g_at_command_state = AT_COMAND_STATE_PARSE_DONE;
            }
            break;
        }
        default: {
            break;
        }
    }
}
#endif

