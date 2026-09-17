#ifndef BT_HFP_HF_H
#define BT_HFP_HF_H

#include <stdint.h>
#include <stdbool.h>
#include "btstack.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HFP_STATE_IDLE,
    HFP_STATE_CONNECTING_SLC,
    HFP_STATE_SLC_CONNECTED,
    HFP_STATE_INCOMING_CALL,
    HFP_STATE_OUTGOING_CALL,
    HFP_STATE_ACTIVE_CALL,
} dialer_call_state_t;

typedef struct {
    dialer_call_state_t call_state;
    hci_con_handle_t acl_handle;
    bd_addr_t peer_addr;
    char peer_addr_str[18];
    char device_name[64];
    char network_operator[32];
    char caller_id[32];
    char caller_name[64];
    uint8_t signal_strength;
    uint8_t battery_level;
    uint8_t service_status;
    uint8_t speaker_volume;
    uint8_t mic_gain;
    uint8_t negotiated_codec;
    bool is_slc_connected;
    bool is_audio_connected;
} hfp_hf_status_t;

typedef void (*hfp_status_changed_callback_t)(const hfp_hf_status_t *status);

/**
 * @brief Initialize the HFP-HF profile and register SDP service record.
 * @param on_status_changed_cb Callback for state updates.
 */
int bt_hfp_init(hfp_status_changed_callback_t on_status_changed_cb);

/**
 * @brief Update the friendly device name of the connected phone.
 */
void bt_hfp_set_device_name(const char *name);

/**
 * @brief Establish Service Level Connection (SLC) to a remote phone.
 */
int bt_hfp_connect(const bd_addr_t remote_addr);

/**
 * @brief Establish Service Level Connection by string address (e.g. "A8:AB:B5:0C:87:F3").
 */
int bt_hfp_connect_addr_string(const char *addr_str);

/**
 * @brief Retrieve the last paired/connected Bluetooth device address.
 * @return true if a valid address was retrieved from persistent storage.
 */
bool bt_hfp_get_last_device(bd_addr_t out_addr, char *out_addr_str, size_t str_len);

/**
 * @brief Save last connected Bluetooth device address to persistent storage.
 */
void bt_hfp_save_last_device(const char *addr_str);

/**
 * @brief Release Service Level Connection.
 */
int bt_hfp_disconnect(void);

/**
 * @brief Place an outgoing phone call.
 */
int bt_hfp_dial(const char *number);

/**
 * @brief Answer an incoming phone call.
 */
int bt_hfp_answer(void);

/**
 * @brief Terminate active or reject incoming call.
 */
int bt_hfp_hangup(void);

/**
 * @brief Send DTMF tone character (0-9, *, #, A-D).
 */
int bt_hfp_send_dtmf(char code);

/**
 * @brief Manually request synchronous audio connection.
 */
int bt_hfp_establish_audio(void);

/**
 * @brief Manually release synchronous audio connection.
 */
int bt_hfp_release_audio(void);

/**
 * @brief Set speaker volume [0..15].
 */
int bt_hfp_set_speaker_volume(uint8_t volume);

/**
 * @brief Set microphone gain [0..15].
 */
int bt_hfp_set_mic_gain(uint8_t gain);

/**
 * @brief Query current HFP HF state snapshot.
 */
void bt_hfp_get_status(hfp_hf_status_t *out_status);

#ifdef __cplusplus
}
#endif

#endif // BT_HFP_HF_H
