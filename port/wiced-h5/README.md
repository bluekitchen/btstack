# BTstack port for WICED platform using H5 transport and Broadcom/Cypress Bluetooth chipsets.

# BTstack port for WICED platform

Tested with:
- WICED SDK 3.4-6.2.1
- [RedBear Duo](https://redbear.cc/product/wifi-ble/redbear-duo.html): Please install [RedBear WICED Add-On](https://github.com/redbear/WICED-SDK) 
- [Inventek Systems ISM4334x](https://www.inventeksys.com/products-page/wifi-modules/serial-wifi/ism43341-m4g-l44-cu-embedded-serial-to-wifi-ble-nfc-module/) - Please contact Inventek Systems for WICED platform files
- [Inventek Systems ISM4343](https://www.inventeksys.com/wifi/wifi-modules/ism4343-wmb-l151/) (BCM43438 A1) - Please contact Inventek Systems for WICED platform files

To integrate BTstack into the WICED SDK, please move the BTstack project into WICED-SDK-6.2.1/libraries.

Then create projects for BTstack examples in WICED/apps/btstack by running:

	./create_examples.py

Now, the BTstack examples can be build from the WICED root in the same way as other examples, e.g.:

	./make btstack.spp_and_le_counter-RB_DUO

to build the SPP-and-LE-Counter example for the RedBear Duo (or use ISM43340_M4G_L44 / ISM4343_WBM_L151 for the Inventek Systems devices).

See WICED documentation about how to upload the firmware.

It should work with all WICED platforms that contain a Broadcom Bluetooth chipset.

The maximal baud rate is currenty limited to 1 mbps. 

The port uses the generated WIFI address plus 1 as Bluetooth MAC address.
It persists the LE Device DB and Classic Link Keys via the DCT mechanism.

All examples that rovide a GATT Server use the GATT DB in the .gatt file. Therefore you need to run ./update_gatt_db.sh in the apps/btstack/$(EXAMPLE) folder after modifying the .gatt file.


## Additional notes on the H5 port

If the CTR/RTS hardware control lines of the Bluetooth Controller is connected to your MCU, we recommend using the wiced-h4 port instead.
If they are not connected, H5 is required to provide a reliable connection including retransmissions in both directions.

There are a few oddities so far that had to be worked around in H5 mode:

- It does not seem possible to upload the FW Mini Driver a.k.a. patchram a.k.a. init script via H5. BTstack uses btstack_chipset_bcm_download_firmware.c to upload the FW Mini Driver via a minimal H4 implementation, before starting up in H5 mode. BCM/CYP chipsets are able to switch to H5.

- With the AP6212A on the RedBear Duo and the FW Mini Driver from WICED-SDK-3.5.2/libraries/drivers/bluetooth/firmware/43438A1/26MHz/bt_firmware_image.c, the HCI LE Encrypt command to perform an AES128 encryption hangs in H5 (but works in H4). See [Bug Report in Community Forum](https://community.cypress.com/thread/8424) As a work around, BTstack was configured to use a CPU implementation of AES128 (#define HAVE_AES128).

