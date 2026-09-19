#define BTSTACK_FILE__ "mac_probe.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "diag_logger.h"
#include "bt/bt_controller.h"
#include "btstack.h"

#ifdef __APPLE__
#include <libusb.h>
#endif

static btstack_timer_source_t s_timeout_timer;
static btstack_packet_callback_registration_t s_hci_event_registration;
static char s_dongle_name[128] = "Unknown Bluetooth Dongle";
static char s_bt_version[32] = "5.0";
static uint16_t s_vid = 0;
static uint16_t s_pid = 0;
static uint8_t s_usb_bus = 0;
static char s_usb_port[32] = "";
static char s_usb_serial[128] = "";

static void timeout_handler(btstack_timer_source_t *ts) {
    (void)ts;
    fprintf(stderr, "[MAC_PROBE] Timeout waiting for Bluetooth controller to become ready.\n");
    printf("__JSON__{ \"status\": \"error\", \"message\": \"Timeout waiting for Bluetooth controller to become ready\" }__JSON__\n");
    fflush(stdout);
    bt_controller_stop();
    exit(1);
}

static void on_controller_ready(const bd_addr_t local_addr) {
    btstack_run_loop_remove_timer(&s_timeout_timer);
    
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%s", bd_addr_to_str(local_addr));
    
    printf("__JSON__{ \"status\": \"ok\", \"name\": \"%s\", \"mac_address\": \"%s\", \"bluetooth_version\": \"%s\", \"vid\": \"0x%04X\", \"pid\": \"0x%04X\", \"usb_bus\": %u, \"usb_port\": \"%s\", \"usb_serial\": \"%s\" }__JSON__\n",
           s_dongle_name, mac_str, s_bt_version, s_vid, s_pid, s_usb_bus, s_usb_port, s_usb_serial);
    fflush(stdout);
    
    bt_controller_stop();
    exit(0);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void)channel;
    if (packet_type != HCI_EVENT_PACKET || size < 7) return;

    if (hci_event_packet_get_type(packet) == HCI_EVENT_COMMAND_COMPLETE) {
        uint16_t opcode = hci_event_command_complete_get_command_opcode(packet);
        if (opcode == HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION && size >= 7) {
            uint8_t hci_ver = packet[6];
            switch (hci_ver) {
                case 0: snprintf(s_bt_version, sizeof(s_bt_version), "1.0b"); break;
                case 1: snprintf(s_bt_version, sizeof(s_bt_version), "1.1"); break;
                case 2: snprintf(s_bt_version, sizeof(s_bt_version), "1.2"); break;
                case 3: snprintf(s_bt_version, sizeof(s_bt_version), "2.0 + EDR"); break;
                case 4: snprintf(s_bt_version, sizeof(s_bt_version), "2.1 + EDR"); break;
                case 5: snprintf(s_bt_version, sizeof(s_bt_version), "3.0 + HS"); break;
                case 6: snprintf(s_bt_version, sizeof(s_bt_version), "4.0"); break;
                case 7: snprintf(s_bt_version, sizeof(s_bt_version), "4.1"); break;
                case 8: snprintf(s_bt_version, sizeof(s_bt_version), "4.2"); break;
                case 9: snprintf(s_bt_version, sizeof(s_bt_version), "5.0"); break;
                case 10: snprintf(s_bt_version, sizeof(s_bt_version), "5.1"); break;
                case 11: snprintf(s_bt_version, sizeof(s_bt_version), "5.2"); break;
                case 12: snprintf(s_bt_version, sizeof(s_bt_version), "5.3"); break;
                case 13: snprintf(s_bt_version, sizeof(s_bt_version), "5.4"); break;
                default: snprintf(s_bt_version, sizeof(s_bt_version), "5.0+"); break;
            }
        }
    }
}

// Inspect USB devices to determine friendly name, serial number, and VID/PID
static bool inspect_usb_dongle(uint16_t target_vid, uint16_t target_pid, uint8_t target_bus,
                              int target_path_len, const uint8_t *target_ports,
                              uint16_t *out_vid, uint16_t *out_pid, char *out_name, size_t max_len,
                              uint8_t *out_bus, char *out_port, size_t max_port_len,
                              char *out_serial, size_t max_serial_len) {
    libusb_context *ctx = NULL;
    if (libusb_init(&ctx) < 0) return false;

    libusb_device **devs = NULL;
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return false;
    }

    bool found = false;
    for (ssize_t i = 0; i < cnt && !found; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devs[i], &desc) != 0) continue;

        uint8_t dev_bus = libusb_get_bus_number(devs[i]);
        uint8_t p_nums[7] = {0};
        int p_len = libusb_get_port_numbers(devs[i], p_nums, 7);

        // If target port path specified, filter strictly
        if (target_path_len > 0) {
            if (target_bus != 0 && dev_bus != target_bus) continue;
            if (p_len != target_path_len || memcmp(p_nums, target_ports, p_len) != 0) continue;
        }

        // If target VID/PID specified, filter strictly
        if (target_vid != 0 && desc.idVendor != target_vid) continue;
        if (target_pid != 0 && desc.idProduct != target_pid) continue;

        // Check if Bluetooth device
        bool is_bt = (desc.bDeviceClass == 0xE0 && desc.bDeviceSubClass == 0x01 && desc.bDeviceProtocol == 0x01);
        if (!is_bt && (desc.bDeviceClass == 0x00 || desc.bDeviceClass == 0xEF)) {
            struct libusb_config_descriptor *cfg = NULL;
            if (libusb_get_active_config_descriptor(devs[i], &cfg) == 0 && cfg) {
                for (uint8_t if_idx = 0; if_idx < cfg->bNumInterfaces && !is_bt; if_idx++) {
                    const struct libusb_interface *iface = &cfg->interface[if_idx];
                    for (int a = 0; a < iface->num_altsetting; a++) {
                        const struct libusb_interface_descriptor *id = &iface->altsetting[a];
                        if (id->bInterfaceClass == 0xE0 && id->bInterfaceSubClass == 0x01 && id->bInterfaceProtocol == 0x01) {
                            is_bt = true;
                            break;
                        }
                    }
                }
                libusb_free_config_descriptor(cfg);
            }
        }

        // Also check known dongle VIDs/PIDs
        if (!is_bt) {
            if ((desc.idVendor == 0x2357 && desc.idProduct == 0x0604) ||
                (desc.idVendor == 0x0bda && (desc.idProduct == 0x8771 || desc.idProduct == 0xb720)) ||
                (desc.idVendor == 0x0a12 && desc.idProduct == 0x0001) ||
                (desc.idVendor == 0x0b05 && (desc.idProduct == 0x17cb || desc.idProduct == 0x190e)) ||
                (desc.idVendor == 0x0a5c && desc.idProduct == 0x21e8)) {
                is_bt = true;
            }
        }

        if (is_bt) {
            *out_vid = desc.idVendor;
            *out_pid = desc.idProduct;
            *out_bus = dev_bus;

            // Format port path string (e.g. "1.4")
            if (p_len > 0) {
                char tmp[32] = {0};
                int off = 0;
                for (int p = 0; p < p_len; p++) {
                    off += snprintf(tmp + off, sizeof(tmp) - off, "%s%u", (p > 0 ? "." : ""), p_nums[p]);
                }
                snprintf(out_port, max_port_len, "%s", tmp);
            } else {
                snprintf(out_port, max_port_len, "unknown");
            }

            // Try reading string descriptors (product, manufacturer, serial)
            libusb_device_handle *handle = NULL;
            char prod_str[128] = {0};
            char mfg_str[128] = {0};
            char serial_str[128] = {0};
            if (libusb_open(devs[i], &handle) == 0 && handle) {
                if (desc.iProduct) {
                    libusb_get_string_descriptor_ascii(handle, desc.iProduct, (unsigned char*)prod_str, sizeof(prod_str));
                }
                if (desc.iManufacturer) {
                    libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, (unsigned char*)mfg_str, sizeof(mfg_str));
                }
                if (desc.iSerialNumber) {
                    libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (unsigned char*)serial_str, sizeof(serial_str));
                }
                libusb_close(handle);
            }

            snprintf(out_serial, max_serial_len, "%s", serial_str);

            // Friendly name determination & default BT version
            if (desc.idVendor == 0x2357 && desc.idProduct == 0x0604) {
                snprintf(out_name, max_len, "TP-Link UB500 (Realtek RTL8761BU)");
                snprintf(s_bt_version, sizeof(s_bt_version), "5.3");
            } else if (desc.idVendor == 0x0a12 && desc.idProduct == 0x0001) {
                snprintf(out_name, max_len, "TP-Link UB400 / CSR8510 A10");
                snprintf(s_bt_version, sizeof(s_bt_version), "4.0");
            } else if (desc.idVendor == 0x0b05 && desc.idProduct == 0x17cb) {
                snprintf(out_name, max_len, "ASUS USB-BT400 (Broadcom BCM20702)");
                snprintf(s_bt_version, sizeof(s_bt_version), "4.0");
            } else if (desc.idVendor == 0x0b05 && desc.idProduct == 0x190e) {
                snprintf(out_name, max_len, "ASUS USB-BT500 (Realtek RTL8761BU)");
                snprintf(s_bt_version, sizeof(s_bt_version), "5.0");
            } else if (desc.idVendor == 0x0bda && desc.idProduct == 0x8771) {
                snprintf(out_name, max_len, "Generic Realtek RTL8761BU Adapter");
                snprintf(s_bt_version, sizeof(s_bt_version), "5.0");
            } else if (desc.idVendor == 0x0a5c && desc.idProduct == 0x21e8) {
                snprintf(out_name, max_len, "Broadcom BCM20702 Adapter");
                snprintf(s_bt_version, sizeof(s_bt_version), "4.0");
            } else if (strlen(prod_str) > 0) {
                if (strlen(mfg_str) > 0 && strstr(prod_str, mfg_str) == NULL) {
                    snprintf(out_name, max_len, "%s %s", mfg_str, prod_str);
                } else {
                    snprintf(out_name, max_len, "%s", prod_str);
                }
                snprintf(s_bt_version, sizeof(s_bt_version), "5.0");
            } else {
                snprintf(out_name, max_len, "Bluetooth USB Dongle (VID 0x%04X, PID 0x%04X)", desc.idVendor, desc.idProduct);
                snprintf(s_bt_version, sizeof(s_bt_version), "5.0");
            }

            found = true;
        }
    }

    libusb_free_device_list(devs, 1);
    libusb_exit(ctx);
    return found;
}

int main(int argc, char **argv) {
    int timeout_sec = 8;
    uint8_t target_bus = 0;
    int target_path_len = 0;
    uint8_t target_ports[7] = {0};
    uint16_t target_vid = 0;
    uint16_t target_pid = 0;

    // argv[1]: timeout (seconds)
    if (argc > 1) {
        int t = atoi(argv[1]);
        if (t > 0 && t <= 60) timeout_sec = t;
    }

    // argv[2]: target USB bus
    if (argc > 2) {
        target_bus = (uint8_t)atoi(argv[2]);
    }

    // argv[3]: target USB port path (e.g. "1.4" or "2" or "1.2.3")
    if (argc > 3 && strlen(argv[3]) > 0 && strcmp(argv[3], "-") != 0 && strcmp(argv[3], "0") != 0) {
        char *path_copy = strdup(argv[3]);
        char *tok = strtok(path_copy, ".");
        while (tok && target_path_len < 7) {
            target_ports[target_path_len++] = (uint8_t)atoi(tok);
            tok = strtok(NULL, ".");
        }
        free(path_copy);
    }

    // argv[4]: target VID (e.g. 0x2357 or 9047)
    if (argc > 4 && strlen(argv[4]) > 0) {
        target_vid = (uint16_t)strtoul(argv[4], NULL, 0);
    }

    // argv[5]: target PID (e.g. 0x0604 or 1540)
    if (argc > 5 && strlen(argv[5]) > 0) {
        target_pid = (uint16_t)strtoul(argv[5], NULL, 0);
    }

    // Step 1: Check and inspect the target USB Bluetooth dongle
    if (!inspect_usb_dongle(target_vid, target_pid, target_bus, target_path_len, target_ports,
                           &s_vid, &s_pid, s_dongle_name, sizeof(s_dongle_name),
                           &s_usb_bus, s_usb_port, sizeof(s_usb_port),
                           s_usb_serial, sizeof(s_usb_serial))) {
        printf("__JSON__{ \"status\": \"no_device\", \"message\": \"Target Bluetooth USB dongle not detected on the bus\" }__JSON__\n");
        fflush(stdout);
        return 2;
    }

    // Suppress stdout diagnostics from diag_logger to keep output clean
    diag_logger_set_sink(NULL, true);

    // Step 2: Initialize Bluetooth controller targeting the specific device
    int res = bt_controller_init_target(s_dongle_name, s_vid, s_pid, target_bus, target_path_len, target_ports, on_controller_ready);
    if (res < 0) {
        printf("__JSON__{ \"status\": \"error\", \"message\": \"bt_controller_init_target failed with code %d\" }__JSON__\n", res);
        fflush(stdout);
        return 1;
    }

    // Step 3: Register packet handler to capture HCI Read Local Version
    s_hci_event_registration.callback = &packet_handler;
    hci_add_event_handler(&s_hci_event_registration);

    // Step 4: Arm timeout watchdog
    btstack_run_loop_set_timer_handler(&s_timeout_timer, timeout_handler);
    btstack_run_loop_set_timer(&s_timeout_timer, timeout_sec * 1000);
    btstack_run_loop_add_timer(&s_timeout_timer);

    // Step 5: Power on controller
    bt_controller_start();

    // Step 6: Execute run loop (blocks until ready callback or timeout exits)
    bt_controller_run();

    return 0;
}

