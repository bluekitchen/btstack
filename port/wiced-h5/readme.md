# BTstack port for WICED platform using H5 transport and Broadcom/Cypress Bluetooth chipsets.

Only tested on Redbear Duo platform. Please install [RedBear WICED Add-On](https://github.com/redbear/WICED-SDK) first.

To integrate BTstack into the WICED SDK, please move the BTstack project into WICED-SDK-3.5.2/libraries.
Then create projects for BTstack examples in WICED/apps/btstack by running:

	./create_examples.py

Now, the BTstack examples can be build from the WICED root in the same way as other examples, e.g.:

	./make btstack.spp_and_le_counter-RB_DUO

to build the SPP-and-LE-Counter example for the RedBear Duo.

See WICED documentation about how to install it.

It should work with all WICED platforms that contain a Broadcom Bluetooth chipset.

The maximal baud rate is limited to 2 mbps.

The port uses the generated WIFI address plus 1 as Bluetooth MAC address.
It stores Classic Link Keys using the DCT mechanism.

All examples that rovide a GATT Server use the GATT DB in the .gatt file. Therefore you need to run ./update_gatt_db.sh in the apps/btstack/$(EXAMPLE) folder after modifying the .gatt file.

## Notes on the H5 port

If the CTR/RTS hardware control lines of the Bluetooth Controller is connected to your MCU, we recommend using the wiced-h4 port instead.
If they are not connected, H5 is required to provide a reliable connecion including retransmissions in both directions.

There are a few oddities so far that have been worked around in H5 mode:

- It does not seem possible to upload the FW Mini Driver a.k.a. patchram a.k.a. init script via H5. BTstack uses btstack_chipset_bcm_download_firmware.c to upload the FW Mini Driver via a minimal H4 implementation, before starting up in H5 mode. BCM/CYP chipsets able to switch to H5.

- With the AP6212A on the RedBear Duo and the FW Mini Driver from WICED-SDK-3.5.2/libraries/drivers/bluetooth/firmware/43438A1/26MHz/bt_firmware_image.c, the HCI LE Encrypt command to perform an AES128 encryption seems to hang in H5 (but works in H4). As a work around, BTstack was configured to use a CPU implementation of AES128 (#define HAVE_AES128).

