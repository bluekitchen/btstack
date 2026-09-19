#ifndef SCO_AUDIO_H
#define SCO_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include "btstack.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SCO_TX_MODE_SILENCE,
    SCO_TX_MODE_TEST_TONE,
    SCO_TX_MODE_WASAPI_MIC,
    SCO_TX_MODE_LIVE_MIC = SCO_TX_MODE_WASAPI_MIC,
} sco_tx_mode_t;

typedef struct {
    uint32_t rx_packets;
    uint32_t rx_bytes;
    uint32_t tx_packets;
    uint32_t tx_bytes;
    uint32_t errors;
    hci_con_handle_t sco_handle;
    uint8_t link_type;
    uint8_t transmission_interval;
    uint8_t retransmission_window;
    uint16_t rx_packet_length;
    uint16_t tx_packet_length;
    uint8_t air_mode;
    bool is_connected;
    sco_tx_mode_t tx_mode;
    uint16_t peak_rx_amplitude;
    uint16_t peak_tx_amplitude;
} sco_audio_stats_t;

typedef void (*sco_rx_pcm_callback_t)(const int16_t *samples, int num_samples);

/**
 * @brief Initialize the SCO audio subsystem.
 */
void sco_audio_init(void);

/**
 * @brief Set the RX PCM callback for forwarding audio to WASAPI render.
 */
void sco_audio_set_rx_callback(sco_rx_pcm_callback_t rx_cb);

/**
 * @brief Set the TX audio source mode (Silence, 1kHz Test Tone, or WASAPI Mic).
 */
void sco_audio_set_tx_mode(sco_tx_mode_t mode);

/**
 * @brief Set the negotiated audio codec (HFP_CODEC_CVSD = 1, HFP_CODEC_MSBC = 2).
 */
void sco_audio_set_codec(uint8_t negotiated_codec);

/**
 * @brief Push microphone PCM samples (8kHz 16-bit mono) into the SCO TX queue.
 */
int sco_audio_push_tx_samples(const int16_t *samples, int num_samples);

/**
 * @brief Process an incoming HCI SCO packet.
 */
void sco_audio_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

/**
 * @brief Handle HCI event for SCO connection lifecycle and CAN_SEND_NOW.
 */
void sco_audio_hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

/**
 * @brief Trigger SCO packet sending when controller is ready.
 */
void sco_audio_send_next(hci_con_handle_t sco_handle);

/**
 * @brief Retrieve real-time SCO statistics and telemetry.
 */
void sco_audio_get_stats(sco_audio_stats_t *out_stats);

/**
 * @brief Check if an active SCO audio connection exists.
 */
bool sco_audio_is_connected(void);

/**
 * @brief Get the active SCO connection handle.
 */
hci_con_handle_t sco_audio_get_handle(void);

/**
 * @brief Notify SCO subsystem that audio connection was released by HFP.
 */
void sco_audio_on_audio_released(void);

#ifdef __cplusplus
}
#endif

#endif // SCO_AUDIO_H
