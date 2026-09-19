#define BTSTACK_FILE__ "bt_controller.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <usbiodef.h>
#include <winusb.h>
#include <ctype.h>
#include "btstack_tlv_windows.h"
#include "btstack_run_loop_windows.h"
static btstack_tlv_windows_t s_tlv_context;
#else
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <libgen.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include "btstack_tlv_posix.h"
#include "btstack_run_loop_posix.h"
#include "hci_transport_usb.h"
#include <libusb.h>
static btstack_tlv_posix_t s_tlv_context;
// Windows defines MAX_PATH; provide a portable equivalent on POSIX so the
// shared firmware-path resolution code below compiles on macOS/Linux too.
#ifndef MAX_PATH
#ifdef PATH_MAX
#define MAX_PATH PATH_MAX
#else
#define MAX_PATH 1024
#endif
#endif
#endif

#include "bt_controller.h"
#include "hfp_hf.h"

// ---------------------------------------------------------------------------
// Bluetooth chip family classification.
//
// The dialer supports two dongle families with different bring-up needs:
//   - Realtek (RTL8761BU, e.g. TP-Link UB500): requires a firmware blob + config
//     blob upload and a vendor custom pre-init sequence.
//   - CSR (CSR8510, e.g. TP-Link UB400): no firmware upload; a warm-reset vendor
//     init handled by BTstack's CSR chipset driver is enough.
// Anything else falls back to a standard HCI reset (no chipset driver), which is
// the correct default for generic/Broadcom/Intel controllers exposed over H2.
// ---------------------------------------------------------------------------
typedef enum {
    BT_CHIP_UNKNOWN = 0,
    BT_CHIP_REALTEK,
    BT_CHIP_CSR,
} bt_chip_family_t;

// Map a USB vendor id to a chip family. Kept in one place so Windows (SetupAPI
// probe) and POSIX (libusb probe) share identical selection logic.
static bt_chip_family_t bt_chip_family_from_vid(uint16_t vid) {
    switch (vid) {
        case 0x2357: // TP-Link (UB500 / RTL8761BU)
        case 0x0bda: // Realtek Semiconductor
            return BT_CHIP_REALTEK;
        case 0x0a12: // Cambridge Silicon Radio (CSR8510 / TP-Link UB400)
            return BT_CHIP_CSR;
        default:
            return BT_CHIP_UNKNOWN;
    }
}

#include "btstack.h"
#include "btstack_chipset_realtek.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "ble/sm.h"
#include "ble/le_device_db_tlv.h"
#include "bluetooth_data_types.h"

// Forward declaration of USB transport instance (hci_transport_h2_winusb on Windows, hci_transport_h2_libusb on POSIX)
extern const hci_transport_t * hci_transport_usb_instance(void);

static bt_controller_ready_callback_t s_ready_callback = NULL;
// The Bluetooth name advertised to phones is always a fixed, platform-based
// value ("Mac Dialer" on macOS, "PC Dialer" everywhere else) regardless of any
// device_name passed to bt_controller_init.
#ifdef __APPLE__
static char s_device_name[64] = "Mac Dialer";
#else
static char s_device_name[64] = "PC Dialer";
#endif
static bd_addr_t s_local_bd_addr;
static char s_local_bd_addr_str[18] = "00:00:00:00:00:00";
static char s_tlv_db_path[128] = "dialer_btstack_keys.tlv";
static bool s_is_ready = false;
static btstack_packet_callback_registration_t s_hci_event_callback_registration;
static btstack_packet_callback_registration_t s_sm_event_callback_registration;
static bt_controller_inquiry_result_callback_t s_inquiry_result_callback = NULL;
static bt_controller_inquiry_complete_callback_t s_inquiry_complete_callback = NULL;
static bool s_is_discovering = false;

// Adapter readiness watchdog: if the HCI never reaches WORKING (e.g. the dongle
// is held by another OS driver on macOS), power-cycle the controller a few times
// and notify the host. Everything here is inert on a normal successful power-on
// because the timer is cancelled the moment WORKING arrives.
static bt_controller_status_callback_t s_status_callback = NULL;
static btstack_timer_source_t s_ready_watchdog;
static bool s_ready_watchdog_active = false;
static int s_power_on_attempts = 0;
// True when the USB probe positively identified a Bluetooth radio on the bus.
// If it stays false, a power-on failure almost certainly means no dongle is
// plugged in (rather than one being held by the OS), so the user-facing message
// can be more accurate.
static bool s_dongle_detected = false;
#define BT_READY_TIMEOUT_MS 8000
#define BT_MAX_POWER_ON_ATTEMPTS 4

static void cancel_ready_watchdog(void);

// Filter devices by Bluetooth Class of Device (CoD).
// Only phones (major device class 0x02) or devices indicating Telephony service (0x200000)
// are relevant. Unreported/0 CoD is allowed (e.g. certain iPhones in partial EIR).
static bool is_phone_cod(uint32_t cod) {
    if (cod == 0) return true;
    uint8_t major_device = (cod >> 8) & 0x1F;
    if (major_device == 0x02) return true; // Cellular / Smart Phone
    if (cod & 0x200000) return true;       // Telephony service class
    return false;                          // Computers (0x01), Audio/Headsets (0x04), Peripherals (0x05), etc.
}

#define MAX_PENDING_REMOTE_NAMES 32
typedef struct {
    bd_addr_t addr;
    uint8_t page_scan_repetition_mode;
    uint16_t clock_offset;
    uint32_t cod;
    int8_t rssi;
    bool in_progress;
} pending_name_req_t;

static pending_name_req_t s_pending_names[MAX_PENDING_REMOTE_NAMES];
static int s_pending_name_count = 0;
static bool s_name_request_in_flight = false;

// ---------------------------------------------------------------------------
// Resolved-name cache (address -> friendly name)
//
// Device names arrive inconsistently: sometimes in the inquiry EIR, sometimes
// only via a follow-up remote-name request that may be delayed, may fail, or may
// not happen at all on a given scan. Without a cache the UI flip-flops between a
// real name and a placeholder across repeated scans/reconnects. This cache is the
// single source of truth: once a real name is known for an address it is kept and
// reused, and never regresses to a placeholder.
// ---------------------------------------------------------------------------
#define MAX_CACHED_NAMES 64
typedef struct {
    bd_addr_t addr;
    char name[64];
    bool used;
} cached_name_t;

static cached_name_t s_name_cache[MAX_CACHED_NAMES];

// Return the cached name for addr, or NULL if none known.
static const char *name_cache_get(const bd_addr_t addr) {
    for (int i = 0; i < MAX_CACHED_NAMES; i++) {
        if (s_name_cache[i].used && bd_addr_cmp(s_name_cache[i].addr, addr) == 0) {
            return s_name_cache[i].name;
        }
    }
    return NULL;
}

// Store/refresh a real (non-empty) name for addr. Ignores empty/placeholder input.
static void name_cache_put(const bd_addr_t addr, const char *name) {
    if (!name || name[0] == '\0') return;
    // Update existing entry.
    for (int i = 0; i < MAX_CACHED_NAMES; i++) {
        if (s_name_cache[i].used && bd_addr_cmp(s_name_cache[i].addr, addr) == 0) {
            snprintf(s_name_cache[i].name, sizeof(s_name_cache[i].name), "%s", name);
            return;
        }
    }
    // Insert into a free slot.
    for (int i = 0; i < MAX_CACHED_NAMES; i++) {
        if (!s_name_cache[i].used) {
            memcpy(s_name_cache[i].addr, addr, sizeof(bd_addr_t));
            snprintf(s_name_cache[i].name, sizeof(s_name_cache[i].name), "%s", name);
            s_name_cache[i].used = true;
            return;
        }
    }
    // Cache full: overwrite slot 0 (simple, rare — the working set is tiny).
    memcpy(s_name_cache[0].addr, addr, sizeof(bd_addr_t));
    snprintf(s_name_cache[0].name, sizeof(s_name_cache[0].name), "%s", name);
    s_name_cache[0].used = true;
}

static void trigger_next_name_request(void) {
    if (s_name_request_in_flight) return;
    for (int i = 0; i < s_pending_name_count; i++) {
        if (!s_pending_names[i].in_progress) {
            s_pending_names[i].in_progress = true;
            s_name_request_in_flight = true;
            printf("[BT_CONTROLLER] Requesting remote name for %s...\n", bd_addr_to_str(s_pending_names[i].addr));
            gap_remote_name_request(s_pending_names[i].addr, s_pending_names[i].page_scan_repetition_mode, s_pending_names[i].clock_offset | 0x8000);
            return;
        }
    }
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event = hci_event_packet_get_type(packet);
    switch (event) {
        case HCI_EVENT_TRANSPORT_USB_INFO: {
            uint16_t vid = hci_event_transport_usb_info_get_vendor_id(packet);
            uint16_t pid = hci_event_transport_usb_info_get_product_id(packet);
            printf("[BT_CONTROLLER] USB Device detected: VID 0x%04x, PID 0x%04x\n", vid, pid);
            break;
        }

        case BTSTACK_EVENT_STATE: {
            uint8_t state = btstack_event_state_get_state(packet);
            if (state == HCI_STATE_WORKING) {
                gap_local_bd_addr(s_local_bd_addr);
                snprintf(s_local_bd_addr_str, sizeof(s_local_bd_addr_str), "%s", bd_addr_to_str(s_local_bd_addr));
                printf("[BT_CONTROLLER] HCI State: WORKING. Local BD_ADDR: %s\n", s_local_bd_addr_str);
                s_is_ready = true;
                // The radio powered on — stop the readiness watchdog so no retry
                // fires, and reset the attempt counter for any future restart.
                cancel_ready_watchdog();
                s_power_on_attempts = 0;

                // Setup persistent TLV database
                snprintf(s_tlv_db_path, sizeof(s_tlv_db_path), "dialer_keys_%s.tlv", bd_addr_to_str_with_delimiter(s_local_bd_addr, '-'));
#ifdef _WIN32
                const btstack_tlv_t * tlv_impl = btstack_tlv_windows_init_instance(&s_tlv_context, s_tlv_db_path);
#else
                const btstack_tlv_t * tlv_impl = btstack_tlv_posix_init_instance(&s_tlv_context, s_tlv_db_path);
#endif
                btstack_tlv_set_instance(tlv_impl, &s_tlv_context);
                const btstack_link_key_db_t * link_key_db = btstack_link_key_db_tlv_get_instance(tlv_impl, &s_tlv_context);
                hci_set_link_key_db(link_key_db);
                le_device_db_tlv_configure(tlv_impl, &s_tlv_context);

                // Configure Classic GAP discovery and connectability
                gap_discoverable_control(1);
                gap_connectable_control(1);
                gap_set_local_name(s_device_name);
                
                // Class of Device: 0x240404 (Telephony + Audio Service, Handsfree / Wearable Headset)
                gap_set_class_of_device(0x240404);
                
                // Link policy: role switch & sniff mode
                gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_ROLE_SWITCH | LM_LINK_POLICY_ENABLE_SNIFF_MODE);
                gap_set_allow_role_switch(true);

                // Secure Simple Pairing
                gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
                gap_ssp_set_auto_accept(1);

                // Setup Dual-mode BLE advertisements (30ms interval) for iOS/Android immediate scan pop-up
                uint8_t adv_data[31];
                uint8_t adv_len = 0;
                adv_data[adv_len++] = 2;
                adv_data[adv_len++] = BLUETOOTH_DATA_TYPE_FLAGS;
                adv_data[adv_len++] = 0x06; // General discoverable, BR/EDR supported

                uint8_t name_len = (uint8_t)strlen(s_device_name);
                if (name_len > 22) name_len = 22;
                adv_data[adv_len++] = name_len + 1;
                adv_data[adv_len++] = BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME;
                memcpy(&adv_data[adv_len], s_device_name, name_len);
                adv_len += name_len;

                bd_addr_t null_addr;
                memset(null_addr, 0, 6);
                gap_advertisements_set_params(0x0030, 0x0030, 0, 0, null_addr, 0x07, 0x00);
                gap_advertisements_set_data(adv_len, adv_data);
                gap_advertisements_enable(1);

                if (s_ready_callback) {
                    s_ready_callback(s_local_bd_addr);
                }
            } else if (state == HCI_STATE_OFF) {
                printf("[BT_CONTROLLER] HCI State: OFF.\n");
                s_is_ready = false;
            }
            break;
        }

        case HCI_EVENT_PIN_CODE_REQUEST: {
            bd_addr_t event_addr;
            hci_event_pin_code_request_get_bd_addr(packet, event_addr);
            printf("[BT_CONTROLLER] PIN code request from %s, responding with '0000'\n", bd_addr_to_str(event_addr));
            gap_pin_code_response(event_addr, "0000");
            break;
        }

        case HCI_EVENT_USER_CONFIRMATION_REQUEST: {
            bd_addr_t event_addr;
            hci_event_user_confirmation_request_get_bd_addr(packet, event_addr);
            uint32_t numeric_value = hci_event_user_confirmation_request_get_numeric_value(packet);
            printf("[BT_CONTROLLER] Auto-confirming SSP pairing with %s (Value: %06u)\n", bd_addr_to_str(event_addr), numeric_value);
            gap_ssp_confirmation_response(event_addr);
            break;
        }

        case GAP_EVENT_PAIRING_COMPLETE: {
            bd_addr_t event_addr;
            gap_event_pairing_complete_get_bd_addr(packet, event_addr);
            uint8_t status = gap_event_pairing_complete_get_status(packet);
            if (status == ERROR_CODE_SUCCESS) {
                printf("[BT_CONTROLLER] Pairing COMPLETE with %s (Success)\n", bd_addr_to_str(event_addr));
            } else {
                printf("[BT_CONTROLLER] Pairing FAILED with %s (Status: 0x%02x)\n", bd_addr_to_str(event_addr), status);
            }
            break;
        }

        case GAP_EVENT_INQUIRY_RESULT: {
            bd_addr_t dev_addr;
            gap_event_inquiry_result_get_bd_addr(packet, dev_addr);
            char addr_buf[18];
            snprintf(addr_buf, sizeof(addr_buf), "%s", bd_addr_to_str(dev_addr));
            char name_buf[64] = "";
            const char *name = "";     // empty = name not yet known (UI decides label)
            bool has_name = false;
            if (gap_event_inquiry_result_get_name_available(packet)) {
                int nlen = gap_event_inquiry_result_get_name_len(packet);
                const uint8_t *name_data = gap_event_inquiry_result_get_name(packet);
                if (nlen > 0 && nlen < (int)sizeof(name_buf)) {
                    memcpy(name_buf, name_data, nlen);
                    name_buf[nlen] = '\0';
                    name = name_buf;
                    has_name = true;
                    name_cache_put(dev_addr, name);   // remember for future scans
                }
            }
            // Fall back to a previously resolved name so the UI never regresses
            // to a placeholder once a real name has been seen.
            if (!has_name) {
                const char *cached = name_cache_get(dev_addr);
                if (cached && cached[0]) {
                    name = cached;
                    has_name = true;
                }
            }
            uint32_t cod = gap_event_inquiry_result_get_class_of_device(packet);
            if (!is_phone_cod(cod)) {
                // Ignore non-phone devices (smart TVs, laptops, Bluetooth speakers/mice, etc.)
                break;
            }
            int8_t rssi = gap_event_inquiry_result_get_rssi_available(packet) ? gap_event_inquiry_result_get_rssi(packet) : 0;

            // Emit result to UI immediately (empty name if still unknown — the UI
            // shows a neutral label and this entry is refreshed once the name
            // resolves, rather than persisting a wrong "Bluetooth Phone").
            if (s_inquiry_result_callback) {
                s_inquiry_result_callback(addr_buf, name, cod, rssi);
            }

            // Enqueue remote name request if name wasn't in EIR and not cached
            if (!has_name) {
                bool already_queued = false;
                for (int i = 0; i < s_pending_name_count; i++) {
                    if (bd_addr_cmp(dev_addr, s_pending_names[i].addr) == 0) {
                        already_queued = true;
                        break;
                    }
                }
                if (!already_queued && s_pending_name_count < MAX_PENDING_REMOTE_NAMES) {
                    memcpy(s_pending_names[s_pending_name_count].addr, dev_addr, sizeof(bd_addr_t));
                    s_pending_names[s_pending_name_count].page_scan_repetition_mode = gap_event_inquiry_result_get_page_scan_repetition_mode(packet);
                    s_pending_names[s_pending_name_count].clock_offset = gap_event_inquiry_result_get_clock_offset(packet);
                    s_pending_names[s_pending_name_count].cod = cod;
                    s_pending_names[s_pending_name_count].rssi = rssi;
                    s_pending_names[s_pending_name_count].in_progress = false;
                    s_pending_name_count++;
                    trigger_next_name_request();
                }
            }
            break;
        }

        case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE: {
            bd_addr_t dev_addr;
            hci_event_remote_name_request_complete_get_bd_addr(packet, dev_addr);
            uint8_t status = hci_event_remote_name_request_complete_get_status(packet);
            s_name_request_in_flight = false;

            // Recover the cod/rssi captured for this address at inquiry time so the
            // re-emit carries the same metadata (previously it emitted 0/0, which
            // wiped the class-of-device on the UI's second update).
            uint32_t cod = 0;
            int8_t rssi = 0;
            for (int i = 0; i < s_pending_name_count; i++) {
                if (bd_addr_cmp(dev_addr, s_pending_names[i].addr) == 0) {
                    cod = s_pending_names[i].cod;
                    rssi = s_pending_names[i].rssi;
                    // Drop this pending entry — its name is now resolved.
                    for (int j = i; j < s_pending_name_count - 1; j++) {
                        s_pending_names[j] = s_pending_names[j + 1];
                    }
                    s_pending_name_count--;
                    break;
                }
            }

            if (status == ERROR_CODE_SUCCESS) {
                const char *name = hci_event_remote_name_request_complete_get_remote_name(packet);
                char addr_buf[18];
                snprintf(addr_buf, sizeof(addr_buf), "%s", bd_addr_to_str(dev_addr));
                if (name && strlen(name) > 0) {
                    printf("[BT_CONTROLLER] Remote Name resolved for %s: '%s'\n", addr_buf, name);
                    name_cache_put(dev_addr, name);   // authoritative name, remembered
                    // Only update the HFP connected phone name if this remote-name resolution
                    // belongs to the currently connected phone! (Previously this erroneously
                    // called bt_hfp_set_device_name for ANY scanned device).
                    hfp_hf_status_t cur_status;
                    bt_hfp_get_status(&cur_status);
                    if (cur_status.is_slc_connected && bd_addr_cmp(dev_addr, cur_status.peer_addr) == 0) {
                        bt_hfp_set_device_name(name);
                    }
                    if (s_inquiry_result_callback) {
                        s_inquiry_result_callback(addr_buf, name, cod, rssi);
                    }
                }
            }
            trigger_next_name_request();
            if (s_inquiry_complete_callback && !s_is_discovering && !s_name_request_in_flight && s_pending_name_count == 0) {
                s_inquiry_complete_callback();
            }
            break;
        }

        case GAP_EVENT_INQUIRY_COMPLETE: {
            s_is_discovering = false;
            if (s_inquiry_complete_callback && !s_name_request_in_flight && s_pending_name_count == 0) {
                s_inquiry_complete_callback();
            }
            break;
        }

        default:
            break;
    }
}

#ifdef _WIN32
static bool probe_usb_bluetooth_dongle(uint16_t *out_vid, uint16_t *out_pid, char *out_name, size_t out_name_len) {
    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) return false;

    SP_DEVICE_INTERFACE_DATA devIntfData;
    devIntfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    DWORD idx = 0;
    bool found = false;

    while (SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_DEVINTERFACE_USB_DEVICE, idx++, &devIntfData)) {
        DWORD reqSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &devIntfData, NULL, 0, &reqSize, NULL);
        if (reqSize == 0) continue;

        PSP_DEVICE_INTERFACE_DETAIL_DATA detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(reqSize);
        if (!detail) continue;
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &devIntfData, detail, reqSize, NULL, NULL)) {
            // Test opening with WinUSB
            HANDLE hDev = CreateFile(detail->DevicePath, GENERIC_WRITE | GENERIC_READ,
                                     FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
            if (hDev != INVALID_HANDLE_VALUE) {
                WINUSB_INTERFACE_HANDLE winusbHandle;
                if (WinUsb_Initialize(hDev, &winusbHandle)) {
                    USB_DEVICE_DESCRIPTOR devDesc;
                    ULONG bytesRead = 0;
                    if (WinUsb_GetDescriptor(winusbHandle, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0,
                                             (PUCHAR)&devDesc, sizeof(devDesc), &bytesRead) && bytesRead == sizeof(devDesc)) {
                        *out_vid = devDesc.idVendor;
                        *out_pid = devDesc.idProduct;
                        if (out_name) {
                            if (devDesc.idVendor == 0x2357 && devDesc.idProduct == 0x0604) snprintf(out_name, out_name_len, "TP-Link UB500 (RTL8761BU)");
                            else if (devDesc.idVendor == 0x0bda) snprintf(out_name, out_name_len, "Realtek Bluetooth Adapter (0x%04X:0x%04X)", devDesc.idVendor, devDesc.idProduct);
                            else if (devDesc.idVendor == 0x0a12) snprintf(out_name, out_name_len, "CSR Cambridge Silicon Radio (0x%04X:0x%04X)", devDesc.idVendor, devDesc.idProduct);
                            else if (devDesc.idVendor == 0x0a5c) snprintf(out_name, out_name_len, "Broadcom Bluetooth Adapter (0x%04X:0x%04X)", devDesc.idVendor, devDesc.idProduct);
                            else if (devDesc.idVendor == 0x8087) snprintf(out_name, out_name_len, "Intel Wireless Bluetooth (0x%04X:0x%04X)", devDesc.idVendor, devDesc.idProduct);
                            else snprintf(out_name, out_name_len, "USB Bluetooth Adapter (0x%04X:0x%04X)", devDesc.idVendor, devDesc.idProduct);
                        }
                        found = true;
                        WinUsb_Free(winusbHandle);
                        CloseHandle(hDev);
                        free(detail);
                        break;
                    }
                    WinUsb_Free(winusbHandle);
                }
                CloseHandle(hDev);
            }
        }
        free(detail);
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);
    return found;
}
#else
// POSIX (macOS / Linux) equivalent of probe_usb_bluetooth_dongle(): enumerate the
// USB bus with libusb and return the VID/PID of the first device that matches a
// family we know how to bring up (Realtek or CSR). This mirrors the Windows
// SetupAPI probe so the chipset-selection logic below is identical on all
// platforms instead of the previous "assume Realtek" hardcode. Uses a throwaway
// libusb context so it does not interfere with the transport's own libusb usage.
// A USB Bluetooth dongle advertises the Wireless Controller class triple:
//   bDeviceClass    == 0xE0  (Wireless Controller)
//   bDeviceSubClass == 0x01  (RF Controller)
//   bDeviceProtocol == 0x01  (Bluetooth programming)
// This mirrors scan_for_bt_endpoints() in BTstack's libusb transport. Some
// dongles declare the class per-interface (bDeviceClass == 0) instead of on the
// device descriptor, so we also scan the interfaces for the same triple.
static bool usb_device_is_bluetooth(libusb_device *dev, const struct libusb_device_descriptor *desc) {
    if (desc->bDeviceClass == 0xE0 && desc->bDeviceSubClass == 0x01 && desc->bDeviceProtocol == 0x01) {
        return true;
    }
    // Composite device: check each interface's class triple.
    if (desc->bDeviceClass != 0x00 && desc->bDeviceClass != 0xEF /* misc/IAD */) {
        return false;
    }
    struct libusb_config_descriptor *config = NULL;
    if (libusb_get_active_config_descriptor(dev, &config) != 0 || !config) {
        return false;
    }
    bool is_bt = false;
    for (uint8_t i = 0; i < config->bNumInterfaces && !is_bt; i++) {
        const struct libusb_interface *iface = &config->interface[i];
        for (int a = 0; a < iface->num_altsetting; a++) {
            const struct libusb_interface_descriptor *id = &iface->altsetting[a];
            if (id->bInterfaceClass == 0xE0 && id->bInterfaceSubClass == 0x01 && id->bInterfaceProtocol == 0x01) {
                is_bt = true;
                break;
            }
        }
    }
    libusb_free_config_descriptor(config);
    return is_bt;
}

static bool probe_usb_bluetooth_dongle(uint16_t *out_vid, uint16_t *out_pid) {
    libusb_context *ctx = NULL;
    if (libusb_init(&ctx) != 0) {
        return false;
    }

    libusb_device **list = NULL;
    ssize_t count = libusb_get_device_list(ctx, &list);
    bool found = false;

    for (ssize_t i = 0; i < count && !found; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(list[i], &desc) != 0) {
            continue;
        }
        // Only accept actual Bluetooth radios. Matching on vendor id alone is
        // unsafe: Realtek (0x0bda), for example, also ships USB Ethernet/card
        // reader chips that would otherwise be mistaken for a BT dongle and sent
        // Bluetooth firmware. Gate on the Wireless-Controller class triple first.
        if (!usb_device_is_bluetooth(list[i], &desc)) {
            continue;
        }
        *out_vid = desc.idVendor;
        *out_pid = desc.idProduct;
        found = true;
    }

    if (list) {
        libusb_free_device_list(list, 1);
    }
    libusb_exit(ctx);
    return found;
}
#endif

static char s_resolved_fw_path[MAX_PATH];
static char s_resolved_cfg_path[MAX_PATH];

static const char* find_existing_firmware_file(const char* relative_filename, char *out_buf, size_t out_buf_size) {
    char exe_dir[MAX_PATH] = {0};
#ifdef _WIN32
    if (GetModuleFileNameA(NULL, exe_dir, MAX_PATH)) {
        char *last_slash = strrchr(exe_dir, '\\');
        if (!last_slash) last_slash = strrchr(exe_dir, '/');
        if (last_slash) *last_slash = '\0';
    }
#elif defined(__APPLE__)
    {
        char raw[MAX_PATH]; uint32_t sz = sizeof(raw);
        if (_NSGetExecutablePath(raw, &sz) == 0) {
            char tmp[MAX_PATH]; snprintf(tmp, sizeof(tmp), "%s", raw);
            snprintf(exe_dir, sizeof(exe_dir), "%s", dirname(tmp));
        }
    }
#else
    {
        char raw[MAX_PATH];
        ssize_t n = readlink("/proc/self/exe", raw, sizeof(raw) - 1);
        if (n > 0) {
            raw[n] = '\0';
            char tmp[MAX_PATH]; snprintf(tmp, sizeof(tmp), "%s", raw);
            snprintf(exe_dir, sizeof(exe_dir), "%s", dirname(tmp));
        }
    }
#endif

    const char *candidates[8];
    char buf0[MAX_PATH], buf1[MAX_PATH], buf2[MAX_PATH], buf3[MAX_PATH], buf4[MAX_PATH], buf5[MAX_PATH];
    
    snprintf(buf0, sizeof(buf0), "%s/firmware/%s", exe_dir, relative_filename);
    snprintf(buf1, sizeof(buf1), "%s/%s", exe_dir, relative_filename);
    snprintf(buf2, sizeof(buf2), "firmware/%s", relative_filename);
    snprintf(buf3, sizeof(buf3), "./%s", relative_filename);
    snprintf(buf4, sizeof(buf4), "tools/btstack/firmware/%s", relative_filename);
    snprintf(buf5, sizeof(buf5), "../firmware/%s", relative_filename);

    candidates[0] = buf0;
    candidates[1] = buf1;
    candidates[2] = buf2;
    candidates[3] = buf3;
    candidates[4] = buf4;
    candidates[5] = buf5;

    for (int i = 0; i < 6; i++) {
        FILE *f = fopen(candidates[i], "rb");
        if (f) {
            fclose(f);
            strncpy(out_buf, candidates[i], out_buf_size - 1);
            out_buf[out_buf_size - 1] = '\0';
            return out_buf;
        }
    }
    return NULL;
}

int bt_controller_init(const char *device_name, bt_controller_ready_callback_t on_ready_cb) {
    return bt_controller_init_target(device_name, 0, 0, 0, 0, NULL, on_ready_cb);
}

int bt_controller_init_target(const char *device_name, uint16_t vid, uint16_t pid, uint8_t bus, int path_len, const uint8_t *ports, bt_controller_ready_callback_t on_ready_cb) {
    // The advertised Bluetooth name is intentionally fixed per-platform (see
    // s_device_name) and does not follow the caller-supplied device_name.
    UNUSED(device_name);
    s_ready_callback = on_ready_cb;

    uint16_t detected_vid = vid, detected_pid = pid;

#ifdef _WIN32
    char dongle_name[128] = "Standard USB Bluetooth Dongle";

    // 1. Preload WinUSB DLL to ensure runtime lookup succeeds on Windows
    HMODULE hWinUsb = LoadLibraryA("WinUSB.dll");
    if (!hWinUsb) {
        printf("[BT_CONTROLLER] ERROR: Could not load WinUSB.dll (Error: %lu)\n", GetLastError());
        return -1;
    }

    if (detected_vid == 0 && probe_usb_bluetooth_dongle(&detected_vid, &detected_pid, dongle_name, sizeof(dongle_name))) {
        printf("[BT_CONTROLLER] Detected USB Adapter: %s\n", dongle_name);
        s_dongle_detected = true;
    } else if (detected_vid != 0) {
        printf("[BT_CONTROLLER] Targeted USB Adapter: VID 0x%04X, PID 0x%04X\n", detected_vid, detected_pid);
        s_dongle_detected = true;
    } else {
        printf("[BT_CONTROLLER] Searching for connected USB Bluetooth device...\n");
    }

    // 2. Initialize BTstack Memory & Windows Run Loop
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_windows_get_instance());
#else
    // 1. Register USB VID/PID combinations for libusb transport on macOS / POSIX
    hci_transport_usb_add_device(0x2357, 0x0604); // TP-Link UB500 (Realtek RTL8761BU)
    hci_transport_usb_add_device(0x0bda, 0x8771); // Generic Realtek RTL8761BU
    hci_transport_usb_add_device(0x0bda, 0xb720); // Generic Realtek RTL8723BU
    hci_transport_usb_add_device(0x0a12, 0x0001); // CSR8510 (TP-Link UB400 & clones)
    hci_transport_usb_add_device(0x0a5c, 0x21e8); // Broadcom BCM20702
    hci_transport_usb_add_device(0x0b05, 0x17cb); // ASUS USB-BT400

    if (vid != 0 && pid != 0) {
        hci_transport_usb_add_device(vid, pid);
    }

    // 2. Configure target USB bus and port path if specified
    if (path_len > 0 && ports != NULL) {
        printf("[BT_CONTROLLER] Targeting specific USB device at bus %u, path length %d\n", bus, path_len);
        hci_transport_usb_set_bus_and_path(bus, path_len, (uint8_t*)ports);
    }

    // 3. Probe the USB bus if VID/PID was not explicitly passed
    if (detected_vid == 0) {
        if (probe_usb_bluetooth_dongle(&detected_vid, &detected_pid)) {
            printf("[BT_CONTROLLER] Detected USB Adapter: VID 0x%04X, PID 0x%04X\n", detected_vid, detected_pid);
            s_dongle_detected = true;
        } else {
            printf("[BT_CONTROLLER] No USB Bluetooth adapter found on the bus; will default to Realtek bring-up in case one appears.\n");
        }
    } else {
        printf("[BT_CONTROLLER] Using targeted USB Adapter: VID 0x%04X, PID 0x%04X\n", detected_vid, detected_pid);
        s_dongle_detected = true;
    }

    // 4. Initialize BTstack Memory & POSIX Run Loop
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
#endif

    // 3. Initialize HCI with USB Transport
    printf("[BT_CONTROLLER] Initializing USB transport & HCI stack...\n");
    const hci_transport_t * transport = hci_transport_usb_instance();
    if (!transport) {
        printf("[BT_CONTROLLER] ERROR: USB transport instance is NULL\n");
        return -2;
    }
    hci_init(transport, NULL);

    // 4. Configure chipset driver dynamically based on detected hardware.
    //
    // Both Windows (SetupAPI) and POSIX (libusb) now fill detected_vid/pid via
    // probe_usb_bluetooth_dongle(), so the selection below is shared. If nothing
    // was matched (detected_vid == 0) we default to Realtek RTL8761BU bring-up:
    // the TP-Link UB500 is the primary supported dongle and its firmware/config
    // blobs must be loaded for BR/EDR discoverability. Without that the chip
    // powers on unpatched and the phone cannot see the adapter.
    bt_chip_family_t chip = bt_chip_family_from_vid(detected_vid);
#ifndef _WIN32
    if (chip == BT_CHIP_UNKNOWN && detected_vid == 0) {
        // POSIX probe found no known adapter — preserve the previous behaviour and
        // assume the primary Realtek hardware so a UB500 still comes up even if the
        // libusb enumeration missed it (e.g. transient permissions).
        chip = BT_CHIP_REALTEK;
    }
#endif

    switch (chip) {
        case BT_CHIP_REALTEK: {
            printf("[BT_CONTROLLER] Configuring Realtek chipset driver (VID: 0x%04X, PID: 0x%04X)...\n", detected_vid, detected_pid);
            const char *fw_path = find_existing_firmware_file("rtl8761bu_fw.bin", s_resolved_fw_path, sizeof(s_resolved_fw_path));
            if (fw_path) {
                printf("[BT_CONTROLLER] Found Realtek firmware: %s\n", fw_path);
                btstack_chipset_realtek_set_firmware_file_path(fw_path);
            } else {
                printf("[BT_CONTROLLER] WARNING: Realtek firmware blob rtl8761bu_fw.bin not found!\n");
            }

            const char *cfg_path = find_existing_firmware_file("rtl8761bu_config.bin", s_resolved_cfg_path, sizeof(s_resolved_cfg_path));
            if (cfg_path) {
                printf("[BT_CONTROLLER] Found Realtek config: %s\n", cfg_path);
                btstack_chipset_realtek_set_config_file_path(cfg_path);
            }

            btstack_chipset_realtek_set_product_id(detected_pid ? detected_pid : 0x0604);
            hci_set_chipset(btstack_chipset_realtek_instance());
            hci_enable_custom_pre_init();
            break;
        }

        case BT_CHIP_CSR: {
            // CSR8510 (TP-Link UB400 & clones) over USB (H2): use a plain HCI
            // reset, NOT btstack_chipset_csr. That driver targets UART (H4)
            // parts — its init script writes UART PSKEYs (baud rate, RTS/CTS for
            // BCSP) and ends with a vendor WarmReset. On a USB dongle the warm
            // reset makes the chip drop off and re-enumerate; the macOS libusb
            // transport does not re-acquire the re-appeared device, so it stalls
            // ("no connection to an IOService") until the readiness watchdog
            // gives up. A USB CSR8510 comes up correctly with the standard HCI
            // Reset alone, so treat it like any generic USB HCI controller.
            printf("[BT_CONTROLLER] Configuring CSR adapter via standard HCI reset (USB CSR8510, VID: 0x%04X, PID: 0x%04X)...\n", detected_vid, detected_pid);
            hci_set_chipset(NULL);
            break;
        }

        case BT_CHIP_UNKNOWN:
        default: {
            printf("[BT_CONTROLLER] Standard Bluetooth HCI mode active (Standard Reset command) for VID 0x%04X, PID 0x%04X.\n", detected_vid, detected_pid);
            hci_set_chipset(NULL);
            break;
        }
    }

    // 5. Initialize Core Protocols (L2CAP, RFCOMM, SDP, SM)
    printf("[BT_CONTROLLER] Initializing Core Protocols (L2CAP, RFCOMM, SDP, SM)...\n");
    l2cap_init();
    rfcomm_init();
    sdp_init();
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(0);

    // 6. Register HCI Event Handler
    s_hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&s_hci_event_callback_registration);

    s_sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&s_sm_event_callback_registration);

    // 7. Pre-configure GAP discovery parameters before power on
    gap_set_local_name(s_device_name);
    gap_set_class_of_device(0x240404);
    gap_discoverable_control(1);
    gap_connectable_control(1);
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_ROLE_SWITCH | LM_LINK_POLICY_ENABLE_SNIFF_MODE);
    gap_set_allow_role_switch(true);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_ssp_set_auto_accept(1);

    printf("[BT_CONTROLLER] BT controller initialization complete.\n");
    return 0;
}

void bt_controller_set_status_callback(bt_controller_status_callback_t on_status) {
    s_status_callback = on_status;
}

static void notify_status(const char *message, bool retrying) {
    printf("[BT_CONTROLLER] %s\n", message);
    if (s_status_callback) {
        s_status_callback(message, retrying);
    }
}

static void arm_ready_watchdog(void);

// Fires if the HCI has not reached WORKING within BT_READY_TIMEOUT_MS. This is
// the recovery path for the dongle being seized by another OS driver (seen on
// macOS with AppleUSBRealtek8153Patcher): power-cycle and retry a few times,
// then ask the user to re-plug. On a healthy power-on this timer is cancelled
// by the BTSTACK_EVENT_STATE=WORKING handler and never runs.
static void ready_watchdog_handler(btstack_timer_source_t *ts) {
    UNUSED(ts);
    s_ready_watchdog_active = false;
    if (s_is_ready) {
        return; // already came up (belt-and-suspenders)
    }

    if (s_power_on_attempts < BT_MAX_POWER_ON_ATTEMPTS) {
        char msg[192];
        snprintf(msg, sizeof(msg),
                 "Bluetooth adapter did not power on (attempt %d/%d) — it may be held by the OS. Retrying...",
                 s_power_on_attempts, BT_MAX_POWER_ON_ATTEMPTS);
        notify_status(msg, true);
        // Clean power cycle, then retry.
        hci_power_control(HCI_POWER_OFF);
        hci_power_control(HCI_POWER_ON);
        s_power_on_attempts++;
        arm_ready_watchdog();
    } else if (!s_dongle_detected) {
        // The probe never saw a Bluetooth radio on the USB bus, so this is almost
        // certainly "nothing plugged in" (or plugged into a hub that isn't
        // passing it through) rather than the dongle being seized by the OS.
        notify_status("No USB Bluetooth adapter detected. Please plug the USB Bluetooth "
                      "dongle directly into the computer (not through a hub) and try again.", false);
    } else {
        notify_status("Bluetooth adapter is not responding. It may be in use by macOS — "
                      "please unplug and re-plug the USB Bluetooth dongle.", false);
    }
}

static void arm_ready_watchdog(void) {
    if (s_ready_watchdog_active) {
        btstack_run_loop_remove_timer(&s_ready_watchdog);
        s_ready_watchdog_active = false;
    }
    btstack_run_loop_set_timer_handler(&s_ready_watchdog, &ready_watchdog_handler);
    btstack_run_loop_set_timer(&s_ready_watchdog, BT_READY_TIMEOUT_MS);
    btstack_run_loop_add_timer(&s_ready_watchdog);
    s_ready_watchdog_active = true;
}

// Cancel the readiness watchdog — called once the HCI reaches WORKING.
static void cancel_ready_watchdog(void) {
    if (s_ready_watchdog_active) {
        btstack_run_loop_remove_timer(&s_ready_watchdog);
        s_ready_watchdog_active = false;
    }
}

int bt_controller_start(void) {
    printf("[BT_CONTROLLER] Powering on HCI Controller...\n");
    s_power_on_attempts = 1;
    hci_power_control(HCI_POWER_ON);
    printf("[BT_CONTROLLER] hci_power_control(HCI_POWER_ON) called.\n");
    // Arm the readiness watchdog so a dongle seized by the OS is detected and
    // recovered automatically instead of silently sitting in a non-working state.
    arm_ready_watchdog();
    return 0;
}

void bt_controller_run(void) {
    btstack_run_loop_execute();
}

void bt_controller_stop(void) {
    cancel_ready_watchdog();
    hci_power_control(HCI_POWER_OFF);
}

bool bt_controller_is_ready(void) {
    return s_is_ready;
}

void bt_controller_get_bd_addr(bd_addr_t out_addr) {
    memcpy(out_addr, s_local_bd_addr, sizeof(bd_addr_t));
}

const char* bt_controller_get_bd_addr_string(void) {
    return s_local_bd_addr_str;
}

int bt_controller_start_discovery(bt_controller_inquiry_result_callback_t on_result, bt_controller_inquiry_complete_callback_t on_complete) {
    if (!s_is_ready) return -1;
    s_inquiry_result_callback = on_result;
    s_inquiry_complete_callback = on_complete;
    s_is_discovering = true;
    s_pending_name_count = 0;
    s_name_request_in_flight = false;
    printf("[BT_CONTROLLER] Starting GAP Classic device discovery (Inquiry duration: ~12.8s)...\n");
    gap_inquiry_start(10);
    return 0;
}

int bt_controller_stop_discovery(void) {
    if (!s_is_discovering) return 0;
    s_is_discovering = false;
    printf("[BT_CONTROLLER] Stopping GAP device discovery...\n");
    gap_inquiry_stop();
    if (s_inquiry_complete_callback) {
        s_inquiry_complete_callback();
    }
    return 0;
}

void bt_controller_request_remote_name(const bd_addr_t addr) {
    if (!s_is_ready) return;
    printf("[BT_CONTROLLER] Issuing direct remote name request for %s...\n", bd_addr_to_str(addr));
    gap_remote_name_request(addr, 0, 0x8000);
}

const char *bt_controller_get_cached_name(const bd_addr_t addr) {
    return name_cache_get(addr);
}

