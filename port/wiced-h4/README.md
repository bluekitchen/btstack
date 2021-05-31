# BTstack Port for WICED platform

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
