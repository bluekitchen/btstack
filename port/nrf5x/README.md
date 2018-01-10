# Experimental port for Nordic nRF5 Series

## Overview

This port targets the bare Nordic nRF5-Series chipsets without the proprietary SoftDevice implementations.

Instead of taking shortcuts within BTstack, the idea here is to provide a complete HCI Controller interface. This requires to implement an HCI Command parser, an LE Link Layer.

## Status

Only tested on the pca10028 dev board.

Only supports LE Scanning at the moment, e.g. with the gap_le_advertisements example.

## Getting Started

To integrate BTstack into the nRF5 SDK, please move the BTstack project into nRF5_SDK_X/components.
Then create projects for the BTstack examples in nRF5_SDK_X/examples/btstack by running:

	./create_examples.py

Now, the BTstack examples can be build from the nRF5_SDK_X examples folder in the same way as other examples, e.g.:

	cd examples/btstack/gap_le_advertisements/pca10028/armgcc
	make

to build the gap_le_advertisements example for the pca10028 dev kit using the ARM GCC compiler.

See nRF5 SDK documentation about how to install it.

