# BTstack port for WICED platform

Only tested on Redbear Duo platform. Please install [RedBear WICED Add-On](https://github.com/redbear/WICED-SDK) first.

To integrate BTstack into the WICED SDK, please move the BTstack project into WICED-SDK-3.5.2/libraries.
Then create projects for BTstack examples in WICED/apps/btstack by running:

	./create_examples.py

Now, the BTstack examples can be build from the WICED root in the same way as other examples, e.g.:

	./make btstack.spp_and_le_counter-RB_DUO

to build the SPP-and-LE-Counter example for the RedBear Duo.

See WICED documentation about how to install it.

It should work with all WICED platforms that contain a Broadcom Bluetooth chipset.

The maximal baud rate is limited to 3 mbps.

The port uses the generated WIFI address plus 1 as Bluetooth MAC address.
It stores Classic Link Keys using the DCT mechanism.

All examples that rovide a GATT Server use the GATT DB in the .gatt file. Therefore you need to run ./update_gatt_db.sh in the apps/btstack/$(EXAMPLE) folder after modifying the .gatt file.
