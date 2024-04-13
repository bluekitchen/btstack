# Java Bindings for BTstack Daemon Client

To build the client library and the examples, run `ant` in this folder.

The code for serialization/deserialization is generated as part of the build process from `btstack/src/btstack_defines.h`

To run one of the examples, the `BTstack Daemon` from `port/daemon` needs to run first.
The daemon needs to be configured for the used operating system and Bluetooth Controller, see `./configure --help`
