# dialer-btstack: Architecture, Implementation Journey & Technical Solutions

## 1. Executive Summary & Goal
The objective of `dialer-btstack` is to provide a standalone, high-performance, native Windows user-mode application that functions as a Bluetooth Hands-Free Profile (HFP-HF 1.8) client. This enables a Windows PC to pair with an iPhone/Android smartphone, initiate and manage cellular phone calls (e.g., dialing, answering, terminating, DTMF), and route **full-duplex bidirectional voice audio** through the PC's built-in speakers and microphone using **WASAPI**.

To bypass Windows OS-level restrictions that prevent Windows from operating as an HFP-HF hands-free unit, the application communicates directly with a dedicated USB Bluetooth 5.0 dongle (**TP-Link UB500 / Realtek RTL8761BU**) using **WinUSB** and **BTstack**.

---

## 2. High-Level Architecture

```
┌────────────────────────────────────────────────────────┐
│               Windows User-Mode Application             │
│    (src/main.c / src/test_main.c / CLI & Telemetry)    │
└──────────────┬──────────────────────────┬──────────────┘
               │                          │
      Audio PCM (8k / 16k)          HFP-HF Control
               │                          │
┌──────────────▼──────────────┐ ┌─────────▼──────────────┐
│        WASAPI Engine        │ │      BTstack Core      │
│  - wasapi_render (Speakers) │ │  - hfp_hf.c (SLC/AT)   │
│  - wasapi_capture (Mic)     │ │  - sco_audio.c         │
│  - Dual-Rate Resampler      │ │  - mSBC / CVSD Codecs  │
└──────────────┬──────────────┘ └─────────┬──────────────┘
               │                          │
               └───────────┬──────────────┘
                           │ HCI SCO / ACL / CMD
┌──────────────────────────▼─────────────────────────────┐
│          WinUSB Transport (hci_transport_h2_winusb)    │
└──────────────────────────┬─────────────────────────────┘
                           │ USB Isochronous & Bulk Endpoints
┌──────────────────────────▼─────────────────────────────┐
│    TP-Link UB500 USB Dongle (Realtek RTL8761BU)        │
│   (VID: 0x2357, PID: 0x0604 / Loaded with Realtek FW)  │
└──────────────────────────┬─────────────────────────────┘
                           │ 2.4 GHz Bluetooth RF
┌──────────────────────────▼─────────────────────────────┐
│                 Smartphone (Cellular Call)             │
└────────────────────────────────────────────────────────┘
```

---

## 3. Initial Challenges, Investigation & Step-by-Step Solutions

### A. Driver Isolation & Silent WinUSB Deployment
- **Problem**: When the TP-Link UB500 is inserted into Windows, the default Windows `BTHUSB.SYS` driver claims the device exclusively. Windows standard Bluetooth stack does not expose HFP Hands-Free client roles to third-party user-mode applications.
- **Solution**:
  - Developed a driver package in `tools/driver_package/` featuring a custom `tplink_winusb.inf` matching hardware ID `USB\VID_2357&PID_0604`.
  - Created a self-signed root certificate (`dialer_driver.cer`) and signed the catalog file (`tplink_winusb.cat`).
  - Automated silent installation via PowerShell script (`tools/install-winusb.ps1`) using `pnputil.exe` and `certutil.exe`.
  - Created a restore script (`tools/restore-bluetooth.ps1`) to seamlessly roll back to standard Windows Bluetooth drivers when desired.

---

### B. BTstack Realtek RTL8761BU Chipset Initialization
- **Problem**: The Realtek RTL8761BU controller boots in a ROM fallback state without firmware and requires a multi-phase initialization sequence (LMP subversion check, baud rate configuration, signature alignment, and downloading firmware patch + config blobs `rtl8761bu_fw.bin` and `rtl8761bu_config.bin`). Furthermore, BTstack's built-in Realtek driver did not recognize TP-Link's specific USB Product ID (`0x0604`).
- **Modifications in `third_party/btstack`**:
  1. **USB PID Registration**:
     In `third_party/btstack/chipset/realtek/btstack_chipset_realtek.c`:
     ```c
     static const patch_info_usb fw_patch_table_usb[] = {
         ...
         {0x8771, 0x8761, "mp_rtl8761b_fw", "rtl8761bu_fw", "rtl8761bu_config", NULL, 0, RTL8761BU},
         {0x0604, 0x8761, "mp_rtl8761b_fw", "rtl8761bu_fw", "rtl8761bu_config", NULL, 0, RTL8761BU}, /* TP-Link UB500 */
     ```
  2. **Custom Firmware Path Handling**:
     Updated `chipset_init()` to check if explicit firmware and configuration paths were already assigned by `bt_controller.c` before falling back to default formatting.
  3. **Controller Firmware Bundling**:
     Placed verified Realtek ROM binaries in `firmware/rtl8761bu_fw.bin` and `firmware/rtl8761bu_config.bin` and bundled them with the executable distribution.

---

### C. HFP-HF 1.8 Profile Implementation
- **Implementation**: In `src/bt/hfp_hf.c`:
  - Implemented the complete Service Level Connection (SLC) state machine.
  - Supported AT command negotiation:
    - Supported Features (`AT+BRSF=1007` -> EC/NR, Three-Way Calling, CLI, Voice Recognition, Wideband Speech, Enhanced Call Status, Codec Negotiation).
    - Codec Selection (`AT+BAC=1,2` -> CVSD & mSBC).
    - AG Indicators (`AT+CIND=?`, `AT+CIND?`, `AT+CMER=3,0,0,1,0`).
    - Call Controls (`ATD<number>;`, `ATA`, `AT+CHUP`, `AT+VTS=<digit>`).
  - Added real-time asynchronous status callbacks for operator name (`+COPS`), signal level (`+CIEV: signal`), battery level (`+CIEV: battchg`), caller ID (`+CLIP`), and call state transitions.

---

### D. Downlink Audio (Speaker Output) Resolution
- **Symptoms**: Call connected and phone routed audio to Bluetooth, but no audio came out of laptop speakers.
- **Root Cause Analysis**:
  1. Windows SDK COM interface retrieval: In pure C, passing `IID_IAudioRenderClient` resulted in `E_NOINTERFACE` (`0x80004002`) due to GUID representation mismatches.
  2. WASAPI Shared Event-Driven Buffering: Calling `IAudioClient::Start()` before populating the initial render buffer caused underruns on certain audio hardware.
  3. Sample Rate Mismatch: Realtek RTL8761BU negotiated Air Mode `0x03` (Transparent / mSBC 16 kHz), whereas the speaker resampler was expecting standard 8 kHz linear PCM.
- **Solution**:
  - Rewrote [wasapi_render.cpp](src/audio/wasapi_render.cpp) as native C++ leveraging `__uuidof(IAudioRenderClient)`.
  - Implemented pre-roll initialization with silent buffer submission prior to starting the client.
  - Built a dynamic polyphase resampler converting from negotiated SCO sample rates (8,000 Hz CVSD or 16,000 Hz mSBC) to native hardware speaker rates (48,000 Hz stereo IEEE Float).

---

### E. Uplink Audio (Microphone Input) Resolution
- **Symptoms**: User could clearly hear peer caller over laptop speakers, but peer caller could not hear anything spoken into laptop microphone (`TX: 0 pkts (0 B)` in telemetry).
- **Root Cause Analysis**:
  1. **HCI Event Dispatch**: BTstack dispatches `HCI_EVENT_SCO_CAN_SEND_NOW` directly to `hci_stack->sco_packet_handler` registered via `hci_register_sco_packet_handler()`. The initial implementation in `sco_audio_packet_handler` dropped all non-`HCI_SCO_DATA_PACKET` packets, preventing the SCO transmit loop from ever triggering.
  2. **Decimation & Resampling Rates**: `wasapi_capture.cpp` was hardcoded to downsample at 8 kHz. Under mSBC wideband speech (16 kHz), feeding half the required sample rate starved the mSBC encoder ring buffer.
  3. **Multi-Channel Microphone Array Decimation**: The laptop microphone array provided 4 channels at 48,000 Hz. Simple decimation caused high-frequency aliasing and frame clipping.
- **Solution**:
  - Handled `HCI_EVENT_SCO_CAN_SEND_NOW` in `sco_audio.c`, executing `sco_audio_send_next()` and chaining `hci_request_sco_can_send_now_event_for_con_handle()` to ensure continuous, uninterrupted transmit flow.
  - Implemented dynamic target capture rate configuration (`wasapi_capture_set_target_sample_rate(16000)` / `8000`).
  - Added box-filter accumulator downsampling across all 4 channels, a 2.5x digital gain boost, and `audio_soft_clip` to provide clear, loud, natural voice transmission.

---

## 4. Summary of Modified Codebase Files

| File | Component | Description |
|---|---|---|
| `third_party/btstack/chipset/realtek/btstack_chipset_realtek.c` | BTstack Realtek Driver | Added TP-Link UB500 PID `0x0604` and explicit firmware path handling |
| `config/btstack_config.h` | Configuration | Configured classic Bluetooth, HFP WBS (mSBC), WinUSB, and TLV storage |
| `src/bt/bt_controller.c` / `.h` | Controller Subsystem | Initializes WinUSB transport, loads Realtek firmware blobs, handles GAP pairing |
| `src/bt/hfp_hf.c` / `.h` | HFP-HF Profile | Manages SLC handshake, indicators, dialing, audio gateways, DTMF |
| `src/bt/sco_audio.c` / `.h` | SCO Audio Pipeline | Implements mSBC encoder/decoder, CVSD routing, TX/RX ring buffers, and telemetry |
| `src/audio/wasapi_render.cpp` / `.h` | Speaker Audio | WASAPI shared-mode render engine with dynamic 8k/16k -> 48k stereo resampling |
| `src/audio/wasapi_capture.cpp` / `.h` | Mic Audio | WASAPI 4-channel microphone capture with anti-aliased downsampling & gain |
| `src/audio/wasapi_common.h` | Audio DSP Utils | Audio ring buffer structures, soft-clipping routines, and format utilities |
| `src/diag_logger.c` / `.h` | Diagnostics | Timestamped session logger and binary PacketLogger (`.pklg`) packet dump engine |
| `src/main.c` | Main Application | Interactive CLI dialer application |
| `src/test_main.c` | Diagnostic Suite | Comprehensive diagnostic test harness with 3-second live telemetry |
| `tools/install-winusb.ps1` | Setup Script | Automated WinUSB driver installation for TP-Link UB500 |
| `tools/restore-bluetooth.ps1` | Rollback Script | Restores default Windows Microsoft Bluetooth driver |

---

## 5. Verification Results

In end-to-end cellular call testing with Airtel `121`:
- **Downlink Audio**: Loud, crystal clear audio delivered to laptop speakers with 0 packet drops.
- **Uplink Audio**: Live voice captured by laptop microphone delivered to peer caller with clear clarity and balanced volume.
- **Telemetric Proof**:
  - Over **9,517 RX packets (228,408 bytes)** received.
  - Over **9,581 TX packets (229,944 bytes)** transmitted.
  - **0 errors / 0 packet losses**.
