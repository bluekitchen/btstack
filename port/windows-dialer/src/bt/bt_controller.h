#ifndef BT_CONTROLLER_H
#define BT_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "btstack.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*bt_controller_ready_callback_t)(const bd_addr_t local_addr);

/**
 * @brief Initialize low-level Bluetooth controller, WinUSB transport, L2CAP, RFCOMM, and SDP.
 * @param device_name Local Bluetooth name advertised to phones.
 * @param on_ready_cb Callback fired when HCI stack is powered on and ready.
 * @return 0 on success, negative error code on failure.
 */
int bt_controller_init(const char *device_name, bt_controller_ready_callback_t on_ready_cb);

/**
 * @brief Power on the Bluetooth controller (call after upper profiles are registered).
 */
int bt_controller_start(void);

/**
 * @brief Start the BTstack main loop (blocking).
 */
void bt_controller_run(void);

/**
 * @brief Request the BTstack run loop to exit and power down.
 */
void bt_controller_stop(void);

/**
 * @brief Check if HCI stack is powered on and ready.
 */
bool bt_controller_is_ready(void);

void bt_controller_get_bd_addr(bd_addr_t out_addr);

/**
 * @brief Get local Bluetooth device address formatted as string (XX:XX:XX:XX:XX:XX).
 */
const char* bt_controller_get_bd_addr_string(void);

typedef void (*bt_controller_inquiry_result_callback_t)(const char *addr_str, const char *name, uint32_t cod, int8_t rssi);
typedef void (*bt_controller_inquiry_complete_callback_t)(void);

/**
 * @brief Start Bluetooth GAP classic device inquiry/discovery.
 */
int bt_controller_start_discovery(bt_controller_inquiry_result_callback_t on_result, bt_controller_inquiry_complete_callback_t on_complete);

/**
 * @brief Stop active Bluetooth GAP device discovery.
 */
int bt_controller_stop_discovery(void);

/**
 * @brief Request friendly name for a specific Bluetooth address.
 */
void bt_controller_request_remote_name(const bd_addr_t addr);

#ifdef __cplusplus
}
#endif

#endif // BT_CONTROLLER_H
