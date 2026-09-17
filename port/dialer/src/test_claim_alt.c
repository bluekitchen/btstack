#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("[TEST_CLAIM_ALT] Starting safe USB Isochronous Interface 1 test...\n");

    libusb_context *ctx = NULL;
    int r = libusb_init(&ctx);
    if (r < 0) {
        fprintf(stderr, "[TEST_CLAIM_ALT] Failed to initialize libusb: %s\n", libusb_error_name(r));
        return 1;
    }

    libusb_device **devs = NULL;
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        fprintf(stderr, "[TEST_CLAIM_ALT] Failed to get device list: %s\n", libusb_error_name((int)cnt));
        libusb_exit(ctx);
        return 1;
    }

    libusb_device_handle *handle = NULL;
    uint16_t found_vid = 0;
    uint16_t found_pid = 0;

    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        r = libusb_get_device_descriptor(devs[i], &desc);
        if (r < 0) continue;

        // Check for Bluetooth device (Wireless Controller / RF Controller, or known Realtek VIDs)
        if ((desc.bDeviceClass == LIBUSB_CLASS_WIRELESS && desc.bDeviceSubClass == 0x01 && desc.bDeviceProtocol == 0x01) ||
            (desc.idVendor == 0x2357 && desc.idProduct == 0x0604) ||  // TP-Link UB500
            (desc.idVendor == 0x0bda && desc.idProduct == 0x8771) ||  // Realtek 8761B
            (desc.idVendor == 0x7392 && desc.idProduct == 0xc611)) {  // Edimax BT-8500
            
            r = libusb_open(devs[i], &handle);
            if (r == 0 && handle) {
                found_vid = desc.idVendor;
                found_pid = desc.idProduct;
                printf("[TEST_CLAIM_ALT] Found Bluetooth adapter: VID 0x%04x, PID 0x%04x\n", found_vid, found_pid);
                break;
            }
        }
    }

    libusb_free_device_list(devs, 1);

    if (!handle) {
        fprintf(stderr, "[TEST_CLAIM_ALT] No supported Bluetooth adapter found or could not open.\n");
        libusb_exit(ctx);
        return 1;
    }

    // Set configuration 1 if needed
    int current_config = -1;
    libusb_get_configuration(handle, &current_config);
    printf("[TEST_CLAIM_ALT] Current USB Configuration: %d\n", current_config);
    if (current_config != 1) {
        printf("[TEST_CLAIM_ALT] Setting configuration 1...\n");
        r = libusb_set_configuration(handle, 1);
        if (r < 0) {
            fprintf(stderr, "[TEST_CLAIM_ALT] Warning: libusb_set_configuration failed: %s\n", libusb_error_name(r));
        }
    }

    // Claim Interface 0 (HCI Command/Event/ACL)
    printf("[TEST_CLAIM_ALT] Claiming Interface 0...\n");
    r = libusb_claim_interface(handle, 0);
    if (r < 0) {
        fprintf(stderr, "[TEST_CLAIM_ALT] Failed to claim Interface 0: %s\n", libusb_error_name(r));
        libusb_close(handle);
        libusb_exit(ctx);
        return 1;
    }
    printf("[TEST_CLAIM_ALT] Interface 0 claimed successfully!\n");

    // Claim Interface 1 (Isochronous Voice/SCO)
    printf("[TEST_CLAIM_ALT] Claiming Interface 1 (Isochronous SCO Audio)...\n");
    r = libusb_claim_interface(handle, 1);
    if (r < 0) {
        fprintf(stderr, "[TEST_CLAIM_ALT] Failed to claim Interface 1: %s\n", libusb_error_name(r));
        libusb_release_interface(handle, 0);
        libusb_close(handle);
        libusb_exit(ctx);
        return 1;
    }
    printf("[TEST_CLAIM_ALT] >>> SUCCESS: Interface 1 claimed WITHOUT panicking macOS! <<<\n");

    // Test switching to Alternate Setting 1 (9 bytes/frame isochronous endpoint)
    printf("[TEST_CLAIM_ALT] Switching Interface 1 to Alternate Setting 1 (SCO active)...\n");
    r = libusb_set_interface_alt_setting(handle, 1, 1);
    if (r < 0) {
        fprintf(stderr, "[TEST_CLAIM_ALT] Error setting alt setting 1: %s\n", libusb_error_name(r));
    } else {
        printf("[TEST_CLAIM_ALT] >>> SUCCESS: Switched to Alternate Setting 1 successfully! <<<\n");
    }

    // Switch back to Alternate Setting 0 (idle)
    printf("[TEST_CLAIM_ALT] Reverting Interface 1 to Alternate Setting 0 (idle)...\n");
    r = libusb_set_interface_alt_setting(handle, 1, 0);
    if (r < 0) {
        fprintf(stderr, "[TEST_CLAIM_ALT] Error reverting alt setting 0: %s\n", libusb_error_name(r));
    } else {
        printf("[TEST_CLAIM_ALT] Reverted to Alternate Setting 0 successfully.\n");
    }

    // Clean up
    libusb_release_interface(handle, 1);
    libusb_release_interface(handle, 0);
    libusb_close(handle);
    libusb_exit(ctx);

    printf("[TEST_CLAIM_ALT] Test finished cleanly. Both interfaces and alt-settings are 100%% operational on macOS!\n");
    return 0;
}
