# `dialer-btstack`
> **Native Windows User-Mode Bluetooth HFP-HF Dialer & Full-Duplex WASAPI Audio Engine**  
> Powered by **BTstack**, **WinUSB** (TP-Link UB500 / Realtek RTL8761BU), and **WASAPI**.

---

## Overview

`dialer-btstack` is a high-performance, lightweight Windows user-mode application that turns a Windows PC into a Bluetooth Hands-Free Unit / Car-Kit for smartphones (iOS and Android). It enables:
1. **Remote Phone Dialing & Call Control**: Standard AT/HFP commands over RFCOMM (`ATD<number>;`, `ATA`, `AT+CHUP`, volume sync, DTMF tones).
2. **Crystal-Clear Speaker Downlink**: Routing caller voice from the smartphone over Bluetooth SCO/eSCO directly to PC speakers via low-latency WASAPI shared mode.
3. **Crystal-Clear Microphone Uplink**: Capturing PC microphone audio via WASAPI, applying 3.5x digital gain staging with soft limiting, and transmitting 46-byte linear PCM frames over USB Isochronous SCO OUT.
4. **$0 Driver Distribution & Zero Kernel Bloat**: Uses Windows inbox `WinUSB.sys` and BTstack's built-in `H2_WINUSB` transport.

---

## Data Path Architecture

```
                 PHONE (iOS / Android)
                           │
                      HFP / RFCOMM
                           │
                      SCO / eSCO (CVSD 8kHz / mSBC 16kHz)
                           │
                   TP-Link UB500 Dongle
                     (Realtek RTL8761BU)
                           │
               USB Isochronous (Interface 1 Alt 5)
                           │
                     Windows WinUSB
                           │
                   BTstack HCI Engine
                 (`H2_WINUSB` Transport)
                           │
          ┌────────────────┴────────────────┐
          │                                 │
       SCO RX                            SCO TX
          │                                 │
     PCM Decoding                      PCM Encoding
          │                                 │
   Audio Resampler                   Audio Resampler
  (8kHz -> 48kHz Stereo)            (48kHz -> 8kHz Mono)
          │                                 │
          ▼                                 ▲
    WASAPI Render                     WASAPI Capture
   (PC Audio DAC)                     (PC Microphone)
```

---

## Project Structure

```text
dialer-btstack/
├── CMakeLists.txt                    # Modern CMake build system (MSVC / Clang)
├── README.md                         # Architecture and documentation
├── config/
│   └── btstack_config.h              # BTstack feature configuration (WinUSB, SCO, HFP)
├── firmware/
│   ├── rtl8761bu_fw.bin              # Realtek RTL8761BU firmware RAM patch
│   └── rtl8761bu_config.bin          # Realtek RTL8761BU controller config
├── third_party/
│   └── btstack/                      # BlueKitchen BTstack library (submodule)
├── src/
│   ├── main.c                        # Production CLI Dialer application (dialer-btstack.exe)
│   ├── test_main.c                   # Phase 1 Diagnostic Test tool (dialer-btstack-test.exe)
│   ├── bt/
│   │   ├── bt_controller.h / .c      # WinUSB transport, Realtek FW loader, GAP discovery
│   │   ├── hfp_hf.h / .c             # HFP-HF profile, SLC handshake, AT commands
│   │   └── sco_audio.h / .c          # SCO RX/TX buffers, 1kHz tone generator, packet alignment
│   └── audio/
│       ├── wasapi_common.h           # Audio ring buffers, linear resamplers, soft limiter
│       ├── wasapi_render.h / .c      # WASAPI speaker playback engine (8kHz -> PC DAC)
│       └── wasapi_capture.h / .c     # WASAPI microphone capture engine (PC Mic -> 8kHz SCO)
└── tools/
    ├── install-winusb.ps1            # 0-prompt automated WinUSB setup script
    ├── restore-bluetooth.ps1         # Rollback script to restore Microsoft inbox driver
    └── driver_package/               # Signed INF/CAT driver package for VID 0x2357 / PID 0x0604
```

---

## Build Instructions

### Prerequisites
- **Visual Studio 2022 / Build Tools** (MSVC C/C++ compiler)
- **CMake 3.20+**

### Compile with CMake
Open PowerShell in the `dialer-btstack` directory:

```powershell
# 1. Generate Visual Studio 2022 project files
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -B build -S . -G "Visual Studio 17 2022" -A x64

# 2. Build Release executables
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build --config Release
```

The resulting executables will be generated in `build/Release/`:
- `build/Release/dialer-btstack-test.exe` (~238 KB)
- `build/Release/dialer-btstack.exe` (~248 KB)

---

## Execution & Verification

### Step 1: Install WinUSB Driver (One-Time)
Run from an elevated PowerShell prompt:
```powershell
.\tools\install-winusb.ps1
```

### Step 2: Phase 1 Diagnostic Verification (`dialer-btstack-test.exe`)
Run the diagnostic test to verify USB endpoints, HCI power-on, Realtek firmware loading, and SCO tone loopback:

```powershell
.\build\Release\dialer-btstack-test.exe
```

**Diagnostic Telemetry:**
```text
================ [DIALER-BTSTACK DIAGNOSTIC TELEMETRY] ================
 USB Controller : DETECTED & ACTIVE (TP-Link UB500 / Realtek RTL8761BU)
 HCI State      : INITIALIZED & WORKING
 Local BD_ADDR  : 0C:EF:15:43:0A:40
----------------------------------------------------------------------
 HFP Profile    : SLC CONNECTED
 Connected Peer : A8:AB:B5:0C:87:F3 (iPhone)
 Network / Op   : Jio 4G (Signal: 4/5, Battery: 4/5)
 Call State     : SLC READY
----------------------------------------------------------------------
 SCO Audio Link : SYNCHRONOUS AUDIO CONNECTED
 SCO Handle     : 0x0002 | Air Mode: CVSD (8 kHz 16-bit PCM)
 SCO Packet Svg : RX: 48 bytes | TX: 48 bytes
 TX Mode        : 1 kHz PCM SINE WAVE TEST TONE
 SCO RX Stats   : 1,420 packets (68,160 bytes)
 SCO TX Stats   : 1,420 packets (68,160 bytes)
======================================================================
```

**Interactive Diagnostic Hotkeys:**
- `t` : Toggle 1 kHz test tone ON / OFF
- `s` : Print instant telemetry statistics
- `b` : Request audio connection (SCO)
- `a` : Answer incoming call
- `h` : Hang up active call
- `q` : Quit test tool

---

### Step 3: Production Dialer Application (`dialer-btstack.exe`)
Run the full-duplex dialer application:

```powershell
.\build\Release\dialer-btstack.exe
```

**Interactive CLI Commands:**
| Command | Description |
| :--- | :--- |
| `status` / `s` | Print full status (adapter, phone, call state, battery, audio counters) |
| `dial <number>` | Place an outgoing call (e.g. `dial 121` or `dial +1234567890`) |
| `answer` / `a` | Answer an incoming ringing call |
| `hangup` / `h` | Terminate active call or reject incoming call |
| `dtmf <char>` | Send DTMF dialpad tone (`0-9`, `*`, `#`) |
| `vol <0-15>` | Adjust PC speaker playback volume |
| `mic <0-15>` | Adjust PC microphone capture gain |
| `tone on\|off` | Switch between 1 kHz test tone and live PC microphone |
| `audio on\|off`| Manually transfer audio connection between PC and phone |
| `connect <addr>`| Connect to a specific phone address (e.g. `connect A8:AB:B5:0C:87:F3`) |
| `disconnect` | Disconnect Service Level Connection from phone |
| `exit` / `q` | Gracefully shutdown dialer and release USB interfaces |

---

## Rollback to Microsoft Bluetooth
To restore the default Windows Bluetooth stack:
```powershell
.\tools\restore-bluetooth.ps1
```
