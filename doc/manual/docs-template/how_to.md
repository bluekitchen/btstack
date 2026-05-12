#

BTstack implements a set of Bluetooth protocols and profiles. To connect to other Bluetooth devices or to provide a Bluetooth services, BTstack has to be properly configured.

The configuration of BTstack is done both at compile time as well as at run time:

- compile time configuration:
    - adjust *btstack_config.h* - this file describes the system configuration, used functionality, and also the memory configuration
    - add necessary source code files to your project

- run time configuration of:
    - Bluetooth chipset
    - run loop
    - HCI transport layer
    - provided services
    - packet handlers

In the following, we provide an overview of the configuration
that is necessary to setup BTstack. From the point when the run loop
is executed, the application runs as a finite
state machine, which processes events received from BTstack. BTstack
groups events logically and provides them via packet handlers.
We provide their overview here. For the case that there is a need to inspect the data exchanged
between BTstack and the Bluetooth chipset, we describe how to configure
packet logging mechanism. Finally, we provide an overview on power management in Bluetooth in general and how to save energy in BTstack.

## Configuration in btstack_config.h {#sec:btstackConfigHowTo}
The file *btstack_config.h* contains three parts:

- \#define HAVE_* directives [listed here](#sec:haveDirectives). These directives describe available system properties, similar to config.h in a autoconf setup.
- \#define ENABLE_* directives [listed here](#sec:enableDirectives). These directives list enabled properties, most importantly ENABLE_CLASSIC and ENABLE_BLE.
- other #define directives for BTstack configuration, most notably static memory, [see next section](#sec:memoryConfigurationHowTo) and [NVM configuration](#sec:nvmConfiguration).

<!-- a name "lst:platformConfiguration"></a-->
<!-- -->

### HAVE_* directives {#sec:haveDirectives}
System properties:

| \#define                        | Description                          |
|---------------------------------|--------------------------------------|
| HAVE_AES128                     | Use platform AES128 engine                                            |
| HAVE_BTSTACK_STDIN              | STDIN is available for CLI interface                                  |
| HAVE_LWIP                       | lwIP is available                    |
| HAVE_MALLOC                     | Use dynamic memory                                                    |
| HAVE_MBEDTLS_ECC_P256           | mbedTLS provides NIST P-256 operations e.g. for LE Secure Connections |

Embedded platform properties:

| \#define                                 | Description                                      |
|------------------------------------------|--------------------------------------------------|
| HAVE_EMBEDDED_TIME_MS                    | System provides time in milliseconds             |
| HAVE_EMBEDDED_TICK                       | System provides tick interrupt                   |
| HAVE_HAL_AUDIO                           | Audio HAL is available                           |
| HAVE_HAL_AUDIO_SINK_<br>BUFFER_CONTEXT   | Audio Sink provides playback time information    |
| HAVE_HAL_AUDIO_SINK_<br>STEREO_ONLY      | Duplicate samples for mono playback              |
| HAVE_HAL_AUDIO_SINK_<br>VOLUME_CONTROL   | Audio Sink provides volume control               |
| HAVE_HAL_AUDIO_SOURCE_<br>BUFFER_CONTEXT | Audio Source provides recording time information |
| HAVE_HAL_AUDIO_SOURCE_<br>GAIN_CONTROL   | Audio Source provides gain control               |

FreeRTOS platform properties:

| \#define                        | Description                          |
|---------------------------------|--------------------------------------|
| HAVE_FREERTOS_<br>INCLUDE_PREFIX    | FreeRTOS headers are in 'freertos' folder (e.g. ESP32's esp-idf) |

POSIX platform properties:

| \#define                        | Description                          |
|---------------------------------|--------------------------------------|
| HAVE_POSIX_FILE_IO              | POSIX File i/o used for hci dump     |
| HAVE_POSIX_TIME                 | System provides time function        |
| LINK_KEY_PATH                   | Path to stored link keys             |
| LE_DEVICE_DB_PATH               | Path to stored LE device information |

<!-- a name "lst:btstackFeatureConfiguration"></a-->
<!-- -->

Chipset properties:

| \#define                        | Description                          |
|---------------------------------|--------------------------------------|
| HAVE_BCM_PCM2                   | PCM2 is used and requires additional configuration    |
| HAVE_BCM_PCM_NBS_16KHZ          | NBS is up/downsampled, use 16 kHz sample rate for NBS |

### ENABLE_* directives {#sec:enableDirectives}
BTstack properties:


| \#define                                                                       | Description                                                                                                                 |
|--------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------|
| ENABLE_A2DP_EXPLICIT_CONFIG                                                    | Let application configure stream endpoint (skip auto-config of SBC endpoint)                                                |
| ENABLE_AIROC_DOWNLOAD_MODE                                                     | Enable AIROC (newer Infineon) Controller PatchRAM download mode                                                             |
| ENABLE_ATT_DELAYED_RESPONSE                                                    | Enable support for delayed ATT operations, see [GATT Server](profiles/#sec:GATTServerProfile)                               |
| ENABLE_AVDTP_ACCEPTOR_<br>EXPLICIT_START_STREAM_<br>CONFIRMATION               | Allow accept or reject of stream start on A2DP_SUBEVENT_<br>START_STREAM_REQUESTED                                          |
| ENABLE_BCM_PCM_WBS                                                             | Enable support for Wide-Band Speech codec in BCM controller, requires<br>ENABLE_SCO_OVER_PCM                                |
| ENABLE_BLE                                                                     | Enable BLE related code in HCI and L2CAP                                                                                    |
| ENABLE_CC256X_ASSISTED_HFP                                                     | Enable support for Assisted HFP mode in CC256x Controller, requires<br>ENABLE_SCO_OVER_PCM                                  |
| ENABLE_CC256X_BAUDRATE_<br>CHANGE_FLOWCONTROL_<br>BUG_WORKAROUND               | Enable workaround for bug in CC256x Flow Control during baud rate change, see chipset docs.                                 |
| ENABLE_CLASSIC                                                                 | Enable Classic related code in HCI and L2CAP                                                                                |
| ENABLE_CLASSIC_OOB_PAIRING                                                     | Enable support for classic Out-of-Band (OOB) pairing                                                                        |
| ENABLE_CONTROLLER_<br>DUMP_PACKETS                                             | Dump number of packets in Controller per type for debugging                                                                 |
| ENABLE_CONTROLLER_<br>WARM_BOOT                                                | Enable stack startup without power cycle (if supported/possible)                                                            |
| ENABLE_CROSS_TRANSPORT_<br>KEY_DERIVATION                                      | Enable Cross-Transport Key Derivation (CTKD) for Secure Connections. Requires BR/EDR plus LE Secure Connections support.    |
| ENABLE_CYPRESS_BAUDRATE_<br>CHANGE_FLOWCONTROL_<br>BUG_WORKAROUND              | Enable workaround for bug in CYW2070x Flow Control during baud rate change, similar to CC256x.                              |
| ENABLE_EHCILL                                                                  | Enable eHCILL low power mode on TI CC256x/WL18xx chipsets                                                                   |
| ENABLE_EXPLICIT_BR_EDR_<br>SECURITY_MANAGER                                    | Report BR/EDR Security Manager support in L2CAP Information Response                                                        |
| ENABLE_EXPLICIT_<br>CONNECTABLE_MODE_CONTROL                                   | Disable calls to control Connectable Mode by L2CAP                                                                          |
| ENABLE_EXPLICIT_DEDICATED_<br>BONDING_DISCONNECT                               | Keep connection after dedicated bonding is complete                                                                         |
| ENABLE_EXPLICIT_IO_<br>CAPABILITIES_REPLY                                      | Let application trigger sending IO Capabilities (Negative) Reply                                                            |
| ENABLE_EXPLICIT_LINK_<br>KEY_REPLY                                             | Let application trigger sending Link Key (Negative) Response, allows for asynchronous link key lookup                       |
| ENABLE_EXPLICIT_PAIRING_ON_SECURITY_REQUEST                                    | Let application trigger LE Pairing upon SM_EVENT_SECURITY_REQUEST                                                           |
| ENABLE_GATT_CLIENT_<br>PAIRING                                                 | Enable GATT Client to start pairing and retry operation on security error                                                   |
| ENABLE_GATT_CLIENT_<br>CACHING                                                 | Enable GATT Service Client to cache Characteristics in TLV                                                                  |
| ENABLE_H5                                                                      | Enable support for SLIP mode in `btstack_uart.h` drivers for HCI H5 ('Three-Wire Mode')                                     |
| ENABLE_HCI_ACL_PACKET_RESERVATION                                              | Allow to reserve ACL packets independent from the stack                                                                     |                                                                    |
| ENABLE_HCI_COMMAND_STATUS_<br>DISCARDED_FOR_FAILED_<br>CONNECTIONS WORKAROUND  | Track connection handle for HCI Commands and assume command has failed if disonnect event for connection is received        |
| ENABLE_HCI_CONTROLLER_<br>TO_HOST_FLOW_CONTROL                                 | Enable HCI Controller to Host Flow Control, see below                                                                       |
| ENABLE_HCI_SERIALIZED_<br>CONTROLLER_OPERATIONS                                | Serialize Inquiry, Remote Name Request, and Create Connection operations                                                    |
| ENABLE_HFP_AT_MESSAGES                                                         | Enable `HFP_SUBEVENT_AT_MESSAGE_SENT` and `HFP_SUBEVENT_AT_MESSAGE_RECEIVED` events                                         |
| ENABLE_HFP_WIDE_BAND_<br>SPEECH                                                | Enable support for mSBC codec used in HFP profile for Wide-Band Speech                                                      |
| ENABLE_L2CAP_ENHANCED_<br>CREDIT_BASED_FLOW_<br>CONTROL_MODE                   | Enable Enhanced credit-based flow-control mode for L2CAP Channels                                                           |
| ENABLE_L2CAP_ENHANCED_<br>RETRANSMISSION_MODE                                  | Enable Enhanced Retransmission Mode for L2CAP Channels. Mandatory for AVRCP Browsing                                        |
| ENABLE_L2CAP_LE_<br>CREDIT_BASED_FLOW_<br>CONTROL_MODE                         | Enable LE credit-based flow-control mode for L2CAP channels                                                                 |
| ENABLE_LE_CENTRAL                                                              | Enable support for LE Central Role in HCI and Security Manager                                                              |
| ENABLE_LE_DATA_<br>LENGTH_EXTENSION                                            | Enable LE Data Length Extension support                                                                                     |
| ENABLE_LE_ENHANCED_<br>CONNECTION_COMPLETE_EVENT                               | Enable LE Enhanced Connection Complete Event v1 & v2                                                                        |
| ENABLE_LE_EXTENDED_<br>ADVERTISING                                             | Enable extended advertising and scanning                                                                                    |
| ENABLE_LE_LIMIT_ACL_<br>FRAGMENT_BY_MAX_OCTETS                                 | Force HCI to fragment ACL-LE packets to fit into over-the-air packet                                                        |
| ENABLE_LE_PERIODIC_<br>ADVERTISING                                             | Enable periodic advertising and scanning                                                                                    |
| ENABLE_LE_PERIPHERAL                                                           | Enable support for LE Peripheral Role in HCI and Security Manager                                                           |
| ENABLE_LE_PRIVACY_<br>ADDRESS_RESOLUTION                                       | Enable address resolution for resolvable private addresses in Controller                                                    |
| ENABLE_LE_PROACTIVE_<br>AUTHENTICATION                                         | Enable automatic encryption for bonded devices on re-connect                                                                |
| ENABLE_LE_SECURE_<br>CONNECTIONS                                               | Enable LE Secure Connections                                                                                                |
| ENABLE_LE_SECURE_<br>CONNECTIONS_DEBUG_KEY                                     | Enable support for LE Secure Connection debug keys for testing                                                              |
| ENABLE_LE_SHORTER_CONNECTION_INTERVALS                                         | Enable LE Shorter Connection Intervals via Frame Space Update and Connection Rate Update                                    |
| ENABLE_LE_SET_<br>ADV_PARAMS_ON_RANDOM_<br>ADDRESS_CHANGE                      | Send HCI LE Set Advertising Params after HCI LE Set Random Address - workaround for Controller Bug                          |
| ENABLE_LE_SIGNED_WRITE                                                         | Enable LE Signed Writes in ATT/GATT                                                                                         |
| ENABLE_LE_WHITELIST<br>_TOUCH_AFTER_<br>RESOLVING_LIST_UPDATE                  | Enable Workaround for Controller bug                                                                                        |
| ENABLE_LOG_BTSTACK_<br>EVENTS                                                  | Log internal/custom BTstack events                                                                                          |
| ENABLE_LOG_DEBUG                                                               | Enable log_debug messages                                                                                                   |
| ENABLE_LOG_ERROR                                                               | Enable log_error messages                                                                                                   |
| ENABLE_LOG_INFO                                                                | Enable log_info messages                                                                                                    |
| ENABLE_MICRO_ECC_FOR_<br>LE_SECURE_CONNECTIONS                                 | Use [micro-ecc library](https://github.com/kmackay/micro-ecc) for ECC operations                                            |
| ENABLE_MODPLAYER                                                               | Enable HXCMOD player in btstack_audio_generator and examples                                                                |
| ENABLE_MUTUAL_<br>AUTHENTICATION_FOR_<br>LEGACY_SECURE_CONNECTIONS             | Re-authentication after connection was encrypted to avoid BIAS Attack. Not needed for min encryption key size of 16         |
| ENABLE_PRINTF_TO_LOG                                                           | Log printf into packet log                                                                                                  |
| ENABLE_RTK_PCM_WBS                                                             | Enable support for Wide-Band Speech codec in Realtek controller, requires ENABLE_SCO_OVER_PCM                               |
| ENABLE_SCO_OVER_HCI                                                            | Enable SCO over HCI for chipsets (if supported)                                                                             |
| ENABLE_SCO_OVER_PCM                                                            | Enable SCO ofer PCM/I2S for chipsets (if supported)                                                                         |
| ENABLE_SEGGER_RTT                                                              | Use SEGGER RTT for console output and packet log, see [additional options](#sec:rttConfiguration)                           |
| ENABLE_TLV_FLASH_<br>EXPLICIT_DELETE_FIELD                                     | Enable use of explicit delete field in TLV Flash implementation - required when flash value cannot be overwritten with zero |
| ENABLE_TLV_FLASH_<br>WRITE_ONCE                                                | Enable storing of emtpy tag instead of overwriting existing tag - required when flash value cannot be overwritten at all    |
| ENABLE_VORBIS                                                                  | Enable OGG-Vorbis support in btstack_audio_genrator and examples                                                            |
Notes:

- ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS: Only some Bluetooth 4.2+ controllers (e.g., EM9304, ESP32) support the necessary HCI commands for ECC. Other reason to enable the ECC software implementations are if the Host is much faster or if the micro-ecc library is already provided (e.g., ESP32, WICED, or if the ECC HCI Commands are unreliable.

### HCI Controller to Host Flow Control
In general, BTstack relies on flow control of the HCI transport, either via Hardware CTS/RTS flow control for UART or regular USB flow control. If this is not possible, e.g on an SoC, BTstack can use HCI Controller to Host Flow Control by defining ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL. If enabled, the HCI Transport implementation must be able to buffer the specified packets. In addition, it also need to be able to buffer a few HCI Events. Using a low number of host buffers might result in less throughput.

Host buffer configuration for HCI Controller to Host Flow Control:

| \#define                | Description                      |
|-------------------------|----------------------------------|
| HCI_HOST_ACL_PACKET_NUM | Max number of ACL packets        |
| HCI_HOST_ACL_PACKET_LEN | Max size of HCI Host ACL packets |
| HCI_HOST_SCO_PACKET_NUM | Max number of ACL packets        |
| HCI_HOST_SCO_PACKET_LEN | Max size of HCI Host SCO packets |

### Memory configuration directives {#sec:memoryConfigurationHowTo}

The structs for services, active connections and remote devices can be
allocated in two different manners:

-   statically from an individual memory pool, whose maximal number of
    elements is defined in the btstack_config.h file. To initialize the static
    pools, you need to call at runtime *btstack_memory_init* function. An example
    of memory configuration for a single SPP service with a minimal
    L2CAP MTU is shown in Listing {@lst:memoryConfigurationSPP}.

-   dynamically using the *malloc/free* functions, if HAVE_MALLOC is
    defined in btstack_config.h file.

For each HCI connection, a buffer of size HCI_ACL_PAYLOAD_SIZE is reserved. For fast data transfer, however, a large ACL buffer of 1021 bytes is recommended. The large ACL buffer is required for 3-DH5 packets to be used.

<!-- a name "lst:memoryConfiguration"></a-->
<!-- -->

| \#define                                  | Description                                                               |
|-------------------------------------------|---------------------------------------------------------------------------|
| HCI_ACL_PAYLOAD_SIZE                      | Max size of HCI ACL payloads                                              |
| HCI_ACL_CHUNK_SIZE_ALIGNMENT              | Alignment of ACL chunk size, can be used to align HCI transport writes    |
| HCI_INCOMING_PRE_BUFFER_SIZE              | Number of bytes reserved before actual data for incoming HCI packets      |
| MAX_NR_BNEP_CHANNELS                      | Max number of BNEP channels                                               |
| MAX_NR_BNEP_SERVICES                      | Max number of BNEP services                                               |
| MAX_NR_GATT_CLIENTS                       | Max number of GATT clients                                                |
| MAX_NR_HCI_CONNECTIONS                    | Max number of HCI connections                                             |
| MAX_NR_HFP_CONNECTIONS                    | Max number of HFP connections                                             |
| MAX_NR_L2CAP_CHANNELS                     | Max number of L2CAP connections                                           |
| MAX_NR_L2CAP_SERVICES                     | Max number of L2CAP services                                              |
| MAX_NR_RFCOMM_CHANNELS                    | Max number of RFCOMM connections                                          |
| MAX_NR_RFCOMM_MULTIPLEXERS                | Max number of RFCOMM multiplexers, with one multiplexer per HCI connection |
| MAX_NR_RFCOMM_SERVICES                    | Max number of RFCOMM services                                             |
| MAX_NR_SERVICE_RECORD_ITEMS               | Max number of SDP service records                                         |
| MAX_NR_SM_LOOKUP_ENTRIES                  | Max number of items in Security Manager lookup queue                      |
| MAX_NR_WHITELIST_ENTRIES                  | Max number of items in GAP LE Whitelist to connect to                     |

The memory is set up by calling *btstack_memory_init* function:

    btstack_memory_init();

<!-- a name "lst:memoryConfigurationSPP"></a-->
<!-- -->

Here's the memory configuration for a basic SPP server.

    #define HCI_ACL_PAYLOAD_SIZE 52
    #define MAX_NR_HCI_CONNECTIONS 1
    #define MAX_NR_L2CAP_SERVICES  2
    #define MAX_NR_L2CAP_CHANNELS  2
    #define MAX_NR_RFCOMM_MULTIPLEXERS 1
    #define MAX_NR_RFCOMM_SERVICES 1
    #define MAX_NR_RFCOMM_CHANNELS 1
    #define NVM_NUM_LINK_KEYS  3

Listing: Memory configuration for a basic SPP server. {#lst:memoryConfigurationSPP}

In this example, the size of ACL packets is limited to the minimum of 52 bytes, resulting in an L2CAP MTU of 48 bytes. Only a singleHCI connection can be established at any time. On it, two L2CAP services are provided, which can be active at the same time. Here, these two can be RFCOMM and SDP. Then, memory for one RFCOMM multiplexer is reserved over which one connection can be active. Finally, up to three link keys can be stored in persistent memory.

<!-- -->

### Non-volatile memory (NVM) directives {#sec:nvmConfiguration}

If implemented, bonding information is stored in Non-volatile memory. For Classic, a single link keys and its type is stored. For LE, the bonding information contains various values (long term key, random number, EDIV, signing counter, identity, ...) Often, this is implemented using Flash memory. Then, the number of stored entries are limited by:

<!-- a name "lst:nvmDefines"></a-->
<!-- -->

| \#define                  | Description                                                                                  |
|---------------------------|----------------------------------------------------------------------------------------------|
| NVM_NUM_LINK_KEYS         | Max number of Classic Link Keys that can be stored                                           |
| NVM_NUM_DEVICE_DB_ENTRIES | Max number of LE Device DB entries that can be stored                                        |
| NVN_NUM_GATT_SERVER_CCC   | Max number of 'Client Characteristic Configuration' values that can be stored by GATT Server |

### HCI Dump Stdout directives {#sec:hciDumpStdout}

Allow to truncate HCI ACL and SCO packets to reduce console output for debugging audio applications.

| \#define                     | Description                               |
|------------------------------|-------------------------------------------|
| HCI_DUMP_STDOUT_MAX_SIZE_ACL | Max size of ACL packets to log via stdout |
| HCI_DUMP_STDOUT_MAX_SIZE_SCO | Max size of SCO packets to log via stdout |
| HCI_DUMP_STDOUT_MAX_SIZE_ISO | Max size of ISO packets to log via stdout |

### SEGGER Real Time Transfer (RTT) directives {#sec:rttConfiguration}

[SEGGER RTT](https://www.segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/) improves on the use of an UART for debugging with higher throughput and less overhead. In addition, it allows for direct logging in PacketLogger/BlueZ format via the provided JLinkRTTLogger tool.

When enabled with `ENABLE_SEGGER_RTT` and `hci_dump_init()` can be called with an `hci_dunp_segger_stdout_get_instance()` for textual output and `hci_dump_segger_binary_get_instance()` for binary output. With the latter, you can select `HCI_DUMP_BLUEZ` or `HCI_DUMP_PACKETLOGGER`, format. For RTT, the following directives are used to configure the up channel:

| \#define                         | Default                       | Description                                                                                                       |
|----------------------------------|-------------------------------|-------------------------------------------------------------------------------------------------------------------|
| SEGGER_RTT_<br>PACKETLOG_MODE        | SEGGER_RTT_<br>MODE_NO_<br>BLOCK_SKIP | SEGGER_RTT_MODE_<br>NO_BLOCK_SKIP to skip messages if buffer is full, or, SEGGER_RTT_MODE_<br>BLOCK_IF_FIFO_FULL to block |
| SEGGER_RTT_<br>PACKETLOG_CHANNEL     | 1                             | Channel to use for packet log. Channel 0 is used for terminal                                                     |
| SEGGER_RTT_<br>PACKETLOG_BUFFER_SIZE | 1024                          | Size of outgoing ring buffer. Increase if you cannot block but get 'message skipped' warnings.                    |

### LE Audio Configuration

The following list of directives are all set with default values. Modify them if you need support for different number of:
- BASS subgroups,
- ASCS Sink and Source endpoints,
- PACS Client Sink and Source Endpoint records,
- number of services for VOCS, or
- if you, in rare case, need to extend the buffers.

| \#define                                                 | Default | Description                                           |
|----------------------------------------------------------|---------|-------------------------------------------------------|
| MAX_SIZE_AICS_AUDIO_<br>INPUT_DESCRIPTION                | 30      | AICS: Size of audio input description.                |
| MAX_NR_ASCS_SERVER_<br>SINK_ENDPOINTS                    | 3       | ASCS Server: Number of Audio Sink endpoints.          |
| MAX_NR_ASCS_SERVER_<br>SOURCE_ENDPOINTS                  | 3       | ASCS Server: Number of Audio Source endpoints.        |
| MAX_NR_ASCS_CLIENT_<br>SINK_ENDPOINTS                    | 3       | ASCS Client: Number of Audio Sink endpoints.          |
| MAX_NR_ASCS_CLIENT_<br>SOURCE_ENDPOINTS                  | 3       | ASCS Client: Number of Audio Source endpoints.        |
| MAX_SIZE_ASCS_SERVER_<br>LONG_WRITE_BUFFER               | 100     | ASCS Server: Size of long write buffer.               |
| MAX_NR_BASS_<br>SUBGROUPS                                | 10      | BASS: Number of subgroups.                            |
| MAX_SIZE_BASS_SERVER_<br>LONG_WRITE_BUFFER               | 512     | BASS Server: Size of long write buffer.               |
| MAX_SIZE_BASS_SERVER_<br>NOTIFICATION_BUFFER             | 200     | BASS Server: Size of notification buffer.             |
| MAX_SIZE_BASS_CLIENT_<br>LONG_WRITE_BUFFER               | 512     | BASS Client: Size of long write buffer.               |
| MAX_SIZE_HAS_<br>PRESET_RECORD_NAME                      | 40      | HAS: Size of preset record name.                      |
| MAX_SIZE_MCS_SERVER_<br>SEARCH_CONTROL_<br>POINT_COMMAND | 100     | MCS Server: Size of search command buffer.            |
| MAX_SIZE_MCS_SERVER_<br>ICON_URL                         | 50      | MCS Server: Size of icon URL.                         |
| MAX_SIZE_MCS_SERVER_<br>TRACK_TITLE                      | 50      | MCS Server: Size of track title.                      |
| MAX_SIZE_MCS_CLIENT_<br>SEARCH_CONTROL_<br>POINT_COMMAND | 100     | MCS Client: Size of search command buffer.            |
| MAX_NR_OTS_SERVER_<br>SERVER_FILTERS                     | 3       | OTS Server: Number of filters.                        |
| MAX_SIZE_OTS_<br>STRING                                  | 60      | OTS Server: Size of string.                           |
| MAX_NR_PACS_CLIENT_<br>SINK_ENDPOINT_RECORDS             | 3       | PACS Client: Number of Audio Sink endpoint records.   |
| MAX_NR_PACS_CLIENT_<br>SOURCE_ENDPOINT_RECORDS           | 3       | PACS Client: Number of Audio Source endpoint records. |
| MAX_SIZE_TBS_CLIENT_<br>SERIALISATION_BUFFER             | 255     | TBS Client: Size of serialization buffer.             |
| MAX_NR_VOCS_SERVER_SERVICES                              | 5       | VOCS Server: Number of services.                      |
| MAX_SIZE_VOCS_<br>AUDIO_OUTPUT_DESCRIPTION               | 30      | VOCS: Size of audio output description.               |
| MAX_NR_TBS_SERVER_<br>URI                                | 252     | TBS Server: Size of URI.                              |
| MAX_NR_TBS_SERVER_<br>PROVIDER_NAME                      | 30      | TBS Server: Size of provider name.                    |
| MAX_NR_TBS_SERVER_<br>UNIFORM_CALLER_IDENTIFIER          | 10      | TBS Server: Size of uniform caller identifier.        |
| MAX_NR_TBS_SERVER_<br>URI_SCHEMES_LIST                   | 30      | TBS Server: Size of URI schemes list.                 |
| MAX_NR_TBS_SERVER_<br>CALL_CONTROL_POINT_NOTIFICATION    | 3       | TBS Server: Size of call control notification.        |
| MAX_SIZE_TBS_CLIENT_<br>SERIALISATION_BUFFER             | 255     | TBS Client: Size of serialisation buffer.             |
_

## Run-time configuration

To allow code-reuse with different platforms as well as with new ports, the low-level initialization of BTstack and
the hardware configuration has been extracted to the *main.c* or similar file in */port/PORT/* folder.
The examples only contain platform-independent Bluetooth logic. 
Let’s have a look at the common init code.

Listing [below](#lst:btstackInit) shows a minimal platform setup for an
embedded system with a Bluetooth chipset connected via UART.

~~~~ {#lst:btstackInit .c caption="{Minimal platform setup for an embedded system}"}

    int main(){
      // ... hardware init: watchdoch, IOs, timers, etc...

      // setup BTstack memory pools
      btstack_memory_init();

      // select embedded run loop
      btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

      // enable logging
      hci_dump_init(hci_dump_embedded_stdout_get_instance());


      // init HCI
      hci_transport_t     * transport = hci_transport_h4_instance();
      hci_init(transport, NULL);

      // setup example
      btstack_main(argc, argv);

      // go
      btstack_run_loop_execute();
    }

~~~~

First, BTstack’s memory pools are set up. Then, the standard run loop
implementation for embedded systems is selected.

The call to *hci_dump_init* configures BTstack to output all Bluetooth
packets and its own debug and error message using printf with BTstack's
millisecond timestamps.s as tim.
The Python
script *tools/create_packet_log.py* can be used to convert the console
output into a Bluetooth PacketLogger format that can be opened by the OS
X PacketLogger tool as well as by Wireshark for further inspection. When
asking for help, please always include a log created with HCI dump.

The *hci_init* function sets up HCI to use the HCI H4 Transport
implementation. It doesn’t provide a special transport configuration nor
a special implementation for a particular Bluetooth chipset. It makes
use of the *remote_device_db_memory* implementation that allows for
re-connects without a new pairing but doesn’t persist the bonding
information.

Finally, it calls *btstack_main()* of the actual example before
executing the run loop.


## Source tree structure {#sec:sourceTreeHowTo}

The source tree has been organized to easily setup new projects.

| Path     | Description                                          |
|----------|------------------------------------------------------|
| chipset  | Support for individual Bluetooth Controller chipsets |
| doc      | Sources for BTstack documentation                    |
| example  | Example applications available for all ports         |
| platform | Support for special OSs and/or MCU architectures     |
| port     | Complete port for a MCU + Chipset combinations       |
| src      | Bluetooth stack implementation                       |
| test     | Unit and PTS tests                                   |
| tool     | Helper tools for BTstack                             |

The core of BTstack, including all protocol and profiles, is in *src/*.

Support for a particular platform is provided by the *platform/* subfolder. For most embedded ports, *platform/embedded/* provides *btstack_run_loop_embedded* and the *hci_transport_h4_embedded* implementation that require *hal_cpu.h*, *hal_led.h*, and *hal_uart_dma.h* plus *hal_tick.h* or *hal_time_ms* to be implemented by the user.

To accommodate a particular Bluetooth chipset, the *chipset/* subfolders provide various btstack_chipset_* implementations.
Please have a look at the existing ports in *port/*.

## Run loop configuration {#sec:runLoopHowTo}

To initialize BTstack you need to [initialize the memory](#sec:memoryConfigurationHowTo)
and [the run loop](#sec:runLoopHowTo) respectively, then setup HCI and all needed higher
level protocols.

BTstack uses the concept of a run loop to handle incoming data and to schedule work.
The run loop handles events from two different types of sources: data
sources and timers. Data sources represent communication interfaces like
an UART or an USB driver. Timers are used by BTstack to implement
various Bluetooth-related timeouts. They can also be used to handle
periodic events. In addition, most implementations also allow to trigger a poll
of the data sources from interrupt context, or, execute a function from a different
thread.

Data sources and timers are represented by the *btstack_data_source_t* and
*btstack_timer_source_t* structs respectively. Each of these structs contain
at least a linked list node and a pointer to a callback function. All active timers
and data sources are kept in link lists. While the list of data sources
is unsorted, the timers are sorted by expiration timeout for efficient
processing. Data sources need to be configured upon what event they are called back.
They can be configured to be polled (*DATA_SOURCE_CALLBACK_POLL*), on read ready (*DATA_SOURCE_CALLBACK_READ*),
or on write ready (*DATA_SOURCE_CALLBACK_WRITE*).

Timers are single shot: a timer will be removed from the timer list
before its event handler callback is executed. If you need a periodic
timer, you can re-register the same timer source in the callback
function, as shown in Listing [PeriodicTimerHandler]. Note that BTstack
expects to get called periodically to keep its time, see Section
[on time abstraction](#sec:timeAbstractionPorting) for more on the
tick hardware abstraction.

BTstack provides different run loop implementations that implement the *btstack_run_loop_t* interface:

- CoreFoundation: implementation for iOS and OS X applications
- Embedded: the main implementation for embedded systems, especially without an RTOS.
- FreeRTOS: implementation to run BTstack on a dedicated FreeRTOS thread
- POSIX: implementation for POSIX systems based on the select() call.
- Qt: implementation for the Qt applications
- WICED: implementation for the Broadcom WICED SDK RTOS abstraction that wraps FreeRTOS or ThreadX.
- Windows: implementation for Windows based on Event objects and WaitForMultipleObjects() call.
- Zephyr: implementation for Zephyr based on k_poll().

Depending on the platform, data sources are either polled (embedded, FreeRTOS), or the platform provides a way
to wait for a data source to become ready for read or write (CoreFoundation, POSIX, Qt, Windows, Zephyr), or,
are not used as the HCI transport driver and the run loop is implemented in a different way (WICED).
In any case, the callbacks must be explicitly enabled with the *btstack_run_loop_enable_data_source_callbacks(..)* function.

In your code, you'll have to configure the run loop before you start it
as shown in Listing [listing:btstackInit]. The application can register
data sources as well as timers, e.g., for periodical sampling of sensors, or
for communication over the UART.

The run loop is set up by calling *btstack_run_loop_init* function and providing
an instance of the actual run loop. E.g. for the embedded platform, it is:

<!-- -->

    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

If the run loop allows to trigger polling of data sources from interrupt context,
*btstack_run_loop_poll_data_sources_from_irq*.

On multi-threaded environments, e.g., FreeRTOS, POSIX, WINDOWS,
*btstack_run_loop_execute_code_on_main_thread* can be used to schedule a callback on the main loop.

The complete Run loop API is provided [here](appendix/apis/#sec:runLoopAPIAppendix).


### Run Loop Embedded

In the embedded run loop implementation, data sources are constantly polled and
the system is put to sleep if no IRQ happens during the poll of all data sources.

The complete run loop cycle looks like this: first, the callback
function of all registered data sources are called in a round robin way.
Then, the callback functions of timers that are ready are executed.
Finally, it will be checked if another run loop iteration has been
requested by an interrupt handler. If not, the run loop will put the MCU
into sleep mode.

Incoming data over the UART, USB, or timer ticks will generate an
interrupt and wake up the microcontroller. In order to avoid the
situation where a data source becomes ready just before the run loop
enters sleep mode, an interrupt-driven data source has to call the
*btstack_run_loop_poll_data_sources_from_irq* function. The call to
*btstack_run_loop_poll_data_sources_from_irq* sets an
internal flag that is checked in the critical section just before
entering sleep mode causing another run loop cycle.

To enable the use of timers, make sure that you defined HAVE_EMBEDDED_TICK or HAVE_EMBEDDED_TIME_MS in the
config file.

While there is no threading, *btstack_run_loop_poll_data_sources_from_irq* allows to reduce stack size by
scheduling a continuation.

### Run Loop FreeRTOS

The FreeRTOS run loop is used on a dedicated FreeRTOS thread and it uses a FreeRTOS queue to schedule callbacks on the run loop.
In each iteration:

- all data sources are polled
- all scheduled callbacks are executed
- all expired timers are called
- finally, it gets the next timeout. It then waits for a 'trigger' or the next timeout, if set.

It supports both *btstack_run_loop_poll_data_sources_from_irq* as well as *btstack_run_loop_execute_code_on_main_thread*.

### Run Loop POSIX

The data sources are standard File Descriptors. In the run loop execute implementation,
select() call is used to wait for file descriptors to become ready to read or write,
while waiting for the next timeout.

To enable the use of timers, make sure that you defined HAVE_POSIX_TIME in the config file.

It supports both *btstack_run_loop_poll_data_sources_from_irq* as well as *btstack_run_loop_execute_code_on_main_thread*.


### Run loop CoreFoundation (OS X/iOS)

This run loop directly maps BTstack's data source and timer source with CoreFoundation objects.
It supports ready to read and write similar to the POSIX implementation. The call to
*btstack_run_loop_execute()* then just calls *CFRunLoopRun()*.

To enable the use of timers, make sure that you defined HAVE_POSIX_TIME in the config file.

It currently only supports *btstack_run_loop_execute_code_on_main_thread*.


### Run Lop Qt

This run loop directly maps BTstack's data source and timer source with Qt Core objects.
It supports ready to read and write similar to the POSIX implementation.

To enable the use of timers, make sure that you defined HAVE_POSIX_TIME in the config file.

It supports both *btstack_run_loop_poll_data_sources_from_irq* as well as *btstack_run_loop_execute_code_on_main_thread*.


### Run loop Windows

The data sources are Event objects. In the run loop implementation WaitForMultipleObjects() call
is all is used to wait for the Event object to become ready while waiting for the next timeout.

It supports both *btstack_run_loop_poll_data_sources_from_irq* as well as *btstack_run_loop_execute_code_on_main_thread*.


### Run loop WICED

WICED SDK API does not provide asynchronous read and write to the UART and no direct way to wait for
one or more peripherals to become ready. Therefore, BTstack does not provide direct support for data sources.
Instead, the run loop provides a message queue that allows to schedule functions calls on its thread via
*btstack_run_loop_wiced_execute_code_on_main_thread()*.

The HCI transport H4 implementation then uses two lightweight threads to do the
blocking read and write operations. When a read or write is complete on
the helper threads, a callback to BTstack is scheduled.

It currently only supports *btstack_run_loop_execute_code_on_main_thread*.


### Run Loop Zephyr

The Zephyr run loop executes BTstack on a Zephyr thread and waits for registered
data sources with *k_poll()*. A BTstack data source handle points to a
*fat_variable_t*, which combines the Zephyr object to wait on, such as a FIFO,
message queue, pipe, semaphore, or poll signal, with the matching
*K_POLL_TYPE_...* identifier. The run loop converts the registered BTstack data
sources into Zephyr *k_poll_event* entries and dispatches the BTstack data source
handler when Zephyr reports the event as ready.

In each iteration:

- expired BTstack timers are processed using Zephyr's *k_uptime_get_32()* clock
- all registered data sources are armed as Zephyr poll events
- the timeout for *k_poll()* is set from the next BTstack timer, or *K_FOREVER*
  if no timer is pending
- ready poll events call the associated BTstack data source with
  *DATA_SOURCE_CALLBACK_READ*

The implementation also registers an internal Zephyr poll signal as a BTstack
data source. This signal wakes the run loop for operations that are not tied to
an external Zephyr object. *btstack_run_loop_poll_data_sources_from_irq* raises
this signal and causes the run loop to poll BTstack data sources from the run
loop thread. *btstack_run_loop_execute_code_on_main_thread* appends the callback
registration to BTstack's callback list while holding a Zephyr mutex, raises the
same signal with a different value, and then the run loop drains and executes the
queued callbacks on the run loop thread.

*btstack_run_loop_trigger_exit* sets an exit flag and raises the signal so that a
run loop blocked in *k_poll()* wakes up and returns. 

It supports both *btstack_run_loop_poll_data_sources_from_irq* as well as *btstack_run_loop_execute_code_on_main_thread*.


## HCI Transport configuration

The HCI initialization has to adapt BTstack to the used platform. The first
call is to *hci_init()* and requires information about the HCI Transport to use.
The arguments are:

-   *HCI Transport implementation*: On embedded systems, a Bluetooth
    module can be connected via USB or an UART port. On embedded, BTstack implements HCI UART Transport Layer (H4) and H4 with eHCILL support, a lightweight low-power variant by Texas Instruments. For POSIX, there is an implementation for HCI H4, HCI H5 and H2 libUSB, and for WICED HCI H4 WICED.
    These are accessed by linking the appropriate file, e.g.,
    [platform/embedded/hci_transport_h4_embedded.c]()
    and then getting a pointer to HCI Transport implementation.
    For more information on adapting HCI Transport to different
    environments, see [here](porting/#sec:hciTransportPorting).

<!-- -->

    hci_transport_t * transport = hci_transport_h4_instance();

-   *HCI Transport configuration*: As the configuration of the UART used
    in the H4 transport interface are not standardized, it has to be
    provided by the main application to BTstack. In addition to the
    initial UART baud rate, the main baud rate can be specified. The HCI
    layer of BTstack will change the init baud rate to the main one
    after the basic setup of the Bluetooth module. A baud rate change
    has to be done in a coordinated way at both HCI and hardware level.
    For example, on the CC256x, the HCI command to change the baud rate
    is sent first, then it is necessary to wait for the confirmation event
    from the Bluetooth module. Only now, can the UART baud rate changed.

<!-- -->

    hci_uart_config_t* config = &hci_uart_config;

After these are ready, HCI is initialized like this:

    hci_init(transport, config);


In addition to these, most UART-based Bluetooth chipset require some
special logic for correct initialization that is not covered by the
Bluetooth specification. In particular, this covers:

- setting the baudrate
- setting the BD ADDR for devices without an internal persistent storage
- upload of some firmware patches.

This is provided by the various *btstack_chipset_t* implementation in the *chipset/* subfolders.
As an example, the *bstack_chipset_cc256x_instance* function returns a pointer to a chipset struct
suitable for the CC256x chipset.

<!-- -->

    btstack_chipset_t * chipset = btstack_chipset_cc256x_instance();
    hci_set_chipset(chipset);


In some setups, the hardware setup provides explicit control of Bluetooth power and sleep modes.
In this case, a *btstack_control_t* struct can be set with *hci_set_control*.

Finally, the HCI implementation requires some form of persistent storage for link keys generated
during either legacy pairing or the Secure Simple Pairing (SSP). This commonly requires platform
specific code to access the MCU’s EEPROM of Flash storage. For the
first steps, BTstack provides a (non) persistent store in memory.
For more see [here](porting/#sec:persistentStoragePorting).

<!-- -->

    btstack_link_key_db_t * link_key_db = &btstack_link_key_db_memory_instance();
    btstack_set_link_key_db(link_key_db);


The higher layers only rely on BTstack and are initialized by calling
the respective *\*_init* function. These init functions register
themselves with the underlying layer. In addition, the application can
register packet handlers to get events and data as explained in the
following section.


## Services {#sec:servicesHowTo}

One important construct of BTstack is *service*. A service represents a
server side component that handles incoming connections. So far, BTstack
provides L2CAP, BNEP, and RFCOMM services. An L2CAP service handles incoming
connections for an L2CAP channel and is registered with its protocol
service multiplexer ID (PSM). Similarly, an RFCOMM service handles
incoming RFCOMM connections and is registered with the RFCOMM channel
ID. Outgoing connections require no special registration, they are
created by the application when needed.


## Packet handlers configuration {#sec:packetHandlersHowTo}


After the hardware and BTstack are set up, the run loop is entered. From
now on everything is event driven. The application calls BTstack
functions, which in turn may send commands to the Bluetooth module. The
resulting events are delivered back to the application. Instead of
writing a single callback handler for each possible event (as it is done
in some other Bluetooth stacks), BTstack groups events logically and
provides them over a single generic interface. Appendix
[Events and Errors](generated/appendix/#sec:eventsAndErrorsAppendix)
summarizes the parameters and event
codes of L2CAP and RFCOMM events, as well as possible errors and the
corresponding error codes.

Here is summarized list of packet handlers that an application might
use:

-   HCI event handler - allows to observer HCI, GAP, and general BTstack events.

-   L2CAP packet handler - handles LE Connection parameter requeset updates

-   L2CAP service packet handler - handles incoming L2CAP connections,
    i.e., channels initiated by the remote.

-   L2CAP channel packet handler - handles outgoing L2CAP connections,
    i.e., channels initiated internally.

-   RFCOMM service packet handler - handles incoming RFCOMM connections,
    i.e., channels initiated by the remote.

-   RFCOMM channel packet handler - handles outgoing RFCOMM connections,
    i.e., channels initiated internally.

These handlers are registered with the functions listed in Table
{@tbl:registeringFunction}.


| Packet Handler                | Registering Function                                                     |
|-------------------------------|--------------------------------------------------------------------------|
| HCI packet handler            | hci_add_event_handler                                                    |
| L2CAP packet handler          | l2cap_register_packet_handler                                            |
| L2CAP service packet handler  | l2cap_register_service                                                   |
| L2CAP channel packet handler  | l2cap_create_channel                                                     |
| RFCOMM service packet handler | rfcomm_register_service and rfcomm_register_service_with_initial_credits |
| RFCOMM channel packet handler | rfcomm_create_channel and rfcomm_create_channel_with_initial_credits     |

Table: Functions for registering packet handlers. {#tbl:registeringFunction}

HCI, GAP, and general BTstack events are delivered to the packet handler
specified by *hci_add_event_handler* function. In L2CAP,
BTstack discriminates incoming and outgoing connections, i.e., event and
data packets are delivered to different packet handlers. Outgoing
connections are used access remote services, incoming connections are
used to provide services. For incoming connections, the packet handler
specified by *l2cap_register_service* is used. For outgoing
connections, the handler provided by *l2cap_create_channel*
is used. RFCOMM and BNEP are similar.

The application can register a single shared packet handler for all
protocols and services, or use separate packet handlers for each
protocol layer and service. A shared packet handler is often used for
stack initialization and connection management.

Separate packet handlers can be used for each L2CAP service and outgoing
connection. For example, to connect with a Bluetooth HID keyboard, your
application could use three packet handlers: one to handle HCI events
during discovery of a keyboard registered by
*l2cap_register_packet_handler*; one that will be registered to an
outgoing L2CAP channel to connect to keyboard and to receive keyboard
data registered by *l2cap_create_channel*; after that
keyboard can reconnect by itself. For this, you need to register L2CAP
services for the HID Control and HID Interrupt PSMs using
*l2cap_register_service*. In this call, you’ll also specify
a packet handler to accept and receive keyboard data.

All events names have the form MODULE_EVENT_NAME now, e.g., *gap_event_-advertising_report*.
To facilitate working with
events and get rid of manually calculating offsets into packets, BTstack provides
auto-generated getters for all fields of all events in *src/hci_event.h*. All
functions are defined as static inline, so they are not wasting any program memory
if not used. If used, the memory footprint should be identical to accessing the
field directly via offsets into the packet. For example, to access fields address_type
and address from the *gap_event_advertising_report* event use following getters:

<!-- -->
    uint8_t address type = gap_event_advertising_report_get_address_type(event);
    bd_addr_t address;
    gap_event_advertising_report_get_address(event, address);


## Bluetooth HCI Packet Logs {#sec:packetlogsHowTo}

If things don't work as expected, having a look at the data exchanged
between BTstack and the Bluetooth chipset often helps.

For this, BTstack provides a configurable packet logging mechanism via hci_dump.h and the following implementations:

    void hci_dump_init(const hci_dump_t * hci_dump_implementation);

| Platform | File                         | Description                                        |
|----------|------------------------------|----------------------------------------------------|
| POSIX    | `hci_dump_posix_fs.c`        | HCI log file for Apple PacketLogger and Wireshark  |
| POSIX    | `hci_dump_posix_stdout.c`    | Console output via printf                          |
| Embedded | `hci_dump_embedded_stdout.c` | Console output via printf                          |
| Embedded | `hci_dump_segger_stdout.c`   | Console output via SEGGER RTT                      |
| Embedded | `hci_dump_segger_binary.c`   | HCI log file for Apple PacketLogger via SEGGER RTT |

On POSIX systems, you can call *hci_dump_init* with a *hci_dump_posix_fs_get_instance()* and
configure the path and output format with *hci_dump_posix_fs_open(const char * path, hci_dump_format_t format)*
where format can be *HCI_DUMP_BLUEZ* or *HCI_DUMP_PACKETLOGGER*.
The resulting file can be analyzed with Wireshark or the Apple's PacketLogger tool.

On embedded systems without a file system, you either log to an UART console via printf or use SEGGER RTT.
For printf output you pass *hci_dump_embedded_stdout_get_instance()* to *hci_dump_init()*.
With RTT, you can choose between textual output similar to printf, and binary output.
For textual output, you can provide the *hci_dump_segger_stdout_get_instance()*.

It will log all HCI packets to the UART console via printf or RTT Terminal.
If you capture the console output, incl. your own debug messages, you can use
the create_packet_log.py tool in the tools folder to convert a text output into a
PacketLogger file.

For less overhead and higher logging speed, you can directly log in binary format by
passing *hci_dump_segger_rtt_binary_get_instance()* and selecting the output format by
calling *hci_dump_segger_rtt_binary_open(hci_dump_format_t format)* with the same format as above.


In addition to the HCI packets, you can also enable BTstack's debug information by adding

    #define ENABLE_LOG_INFO
    #define ENABLE_LOG_ERROR

to the btstack_config.h and recompiling your application.

## LE Security Setup for GATT {#sec:leGattSecurityExamples}

For Bluetooth LE, BTstack separates GATT access control from the actual pairing and encryption procedure:

- the GATT database declares which Characteristics require encryption or authentication
- the ATT server checks those permissions for each read and write request
- the Security Manager performs pairing, bonding, re-encryption, identity resolving, and user interaction

This means that a GATT application usually does not start by "making GATT secure" globally. 
Instead, it marks the attributes that need protection and configures the LE Security Manager so BTstack can raise 
the LE link security when those attributes are accessed.

The main examples to look at are:

- `example/gatt_counter.c` for a small GATT server baseline
- `example/gatt_streamer_server.c` for a GATT server with notifications and higher throughput
- `example/hog_keyboard_demo.c` for a GATT-based profile that requires bonding
- `example/gatt_heart_rate_client.c` for a GATT client
- `example/sm_pairing_central.c` and `example/sm_pairing_peripheral.c` for explicit pairing behavior

### What BTstack Checks on the GATT Server

The `.gatt` file can mark Characteristics with security permissions:

- `READ_ENCRYPTED`
- `READ_AUTHENTICATED`
- `WRITE_ENCRYPTED`
- `WRITE_AUTHENTICATED`
- `AUTHENTICATION_REQUIRED`
- `ENCRYPTION_KEY_SIZE_X`, where `X` is in the range 7..16

For example:

    CHARACTERISTIC, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL, DYNAMIC | READ | NOTIFY | READ_ENCRYPTED,

When an ATT request arrives, BTstack checks the current LE link security before calling the application's read or write callback. 
If the link is not secure enough, the ATT server returns the corresponding ATT security error. 
The remote GATT client can then start pairing or encryption and retry the request.

The important point is that the application callback should not be the first line of defense for normal GATT permissions.
Express the required protection in the `.gatt` database, then keep the callback focused on producing or accepting the attribute value.

### Security Manager Setup

If any GATT Characteristic requires LE security or the remote might try to pair, e.g. when communicating with a smartphone,
you should initialize the Security Manager:

    sm_init();

For production devices, support for LE Secure Connections should be enabled and you should 
configure stable identity and encryption roots with `sm_set_ir()` and `sm_set_er()` as 
described in [SMP - Security Manager Protocol](protocols.md#sec:smpProtocols). These values must survive reboot. 
If they change, previously bonded peers may no longer be recognized correctly.

To enable support for LE Secure Connections, add `ENABLE_LE_SECURE_CONNECTIONS` in `btstack_config.h`.

If activated, BTstack will also enable "LE Secure Connections Only Mode", which will reject Legacy Pairing.
If you need to support peer devices that do not support LE Secure Connections, you can disable Secure Connections
Only Mode by calling:

    sm_set_secure_connections_only_mode(false);

By default, the the IO Capabilites are set to No Input/No Output, which leads to Just Works pairing.

This allows pairing when needed but does not give MITM protection. It is acceptable for simple data where encryption 
is useful but user authentication is not required.

For a product that should remember peers across reconnects, request bonding with support for LE Secure Connections:

    sm_set_authentication_requirements(SM_AUTHREQ_BONDING | SM_AUTHREQ_SECURE_CONNECTION);

For a product that needs protection against active MITM attacks, also request MITM protection and configure IO capabilities 
that can actually support it:

    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_YES_NO);
    sm_set_authentication_requirements(SM_AUTHREQ_MITM_PROTECTION | SM_AUTHREQ_BONDING | SM_AUTHREQ_SECURE_CONNECTION);


### Handling Pairing Events

When `att_server` is used, Security Manager events are delivered through the ATT server packet handler. 
The application should handle the events that match its security policy and user interface.

For Just Works pairing:

    case SM_EVENT_JUST_WORKS_REQUEST:
        sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
        break;

For Numeric Comparison:

    case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
        printf("Confirm value %" PRIu32 "\n",
               sm_event_numeric_comparison_request_get_passkey(packet));
        sm_numeric_comparison_confirm(sm_event_numeric_comparison_request_get_handle(packet));
        break;

For Passkey Entry:

    case SM_EVENT_PASSKEY_INPUT_NUMBER:
        sm_passkey_input(sm_event_passkey_input_number_get_handle(packet), 123456);
        break;

If the user rejects pairing or the application policy does not allow it currenelty, e.g. because the user didn't put 
it into some kind of 'pairing mode', the application can decline the bonding using the connection handle from the pairing event:

    sm_bonding_decline(con_handle);

After pairing finishes, BTstack reports `SM_EVENT_PAIRING_COMPLETE`.
An applications that show a pairing dialog should close it there and check the status before treating the peer as trusted.


### GATT Client Security

By default, the GATT Client sends the requested operation and lets the remote GATT Server enforce security.
If the remote Characteristic requires stronger security, the query completes with an ATT security error.

There are three useful client-side modes:

- reactive: send the GATT request first and handle an insufficient-security ATT error
- reactive with retry: define `ENABLE_GATT_CLIENT_PAIRING` so the GATT Client starts pairing on insufficient-security errors and retries the request
- mandatory: call `gatt_client_set_required_security_level()` so the link is encrypted or authenticated before GATT Client requests are sent

For example, a client that should only talk to a server over an encrypted LE link can require:

    gatt_client_set_required_security_level(LEVEL_2);

Use a higher level if the peer and product need stronger guarantees:

- `LEVEL_2` requires encryption
- `LEVEL_3` requires authenticated pairing
- `LEVEL_4` requires Secure Connections grade protection

If the device is already bonded, `ENABLE_LE_PROACTIVE_AUTHENTICATION` can make BTstack try to re-encrypt the link before GATT Client traffic. If the remote device has lost its bonding information, the GATT query can complete with `ATT_ERROR_BONDING_INFORMATION_MISSING`, and the application should decide whether to delete the local bond and pair again.

### Bonding and Persistence

Pairing only survives reboot if the LE bonding data is stored persistently. For LE, BTstack stores values such as the Long Term Key, Identity Resolving Key, EDIV, random number, and signing counters in the LE device database.

For GATT servers, persistent storage also matters for Client Characteristic Configuration values. Notifications and indications are enabled by clients via CCC descriptors, and bonded clients often expect those settings to survive reconnects. The number of stored CCC values is configured with `NVN_NUM_GATT_SERVER_CCC`.

On embedded targets, LE device data and CCC data are commonly backed by TLV flash storage. On POSIX ports, file-backed storage is usually used.

### Practical Rule

For a secure LE GATT server:

1. Mark protected Characteristics in the `.gatt` file.
2. Call `sm_init()` and configure IO capabilities, bonding, MITM, and Secure Connections policy.
3. Handle Security Manager user interaction events.
4. Persist LE bonding data, and persist CCC values if bonded clients use notifications or indications.

For a secure LE GATT client:

1. Decide whether server-enforced security is enough, or whether the client should require a security level before requests.
2. Use `ENABLE_GATT_CLIENT_PAIRING`, `ENABLE_LE_PROACTIVE_AUTHENTICATION`, and `gatt_client_set_required_security_level()` as appropriate.
3. Handle bonding loss explicitly, especially for products that reconnect to known devices.

## Classic Bluetooth Security Setup in Examples {#sec:classicSecurityExamples}

For BR/EDR (Classic Bluetooth), BTstack uses Security Mode 4 by default:

- `gap_get_security_mode()` defaults to `GAP_SECURITY_MODE_4`
- `gap_get_security_level()` defaults to `LEVEL_2`

From a BTstack application perspective, setting up Classic security consists of four parts:

1. Select the global security policy before initializing services and profiles.
2. Configure Secure Simple Pairing (SSP) IO capabilities and authentication requirements.
3. Handle pairing-related HCI events in the application packet handler.
4. Store link keys in persistent storage if bonding shall survive reboot.

The examples handle pairing-related events explicitly in the application packet handler. Legacy PIN pairing is intentionally rejected by default and reported on stdio, while Secure Simple Pairing (SSP) confirmation requests are reported with the numeric value before the example accepts them. SPP is introduced with the Bluetooth 2.1 specification in 2007 to make device connections both faster and more secure.

If `ENABLE_CROSS_TRANSPORT_KEY_DERIVATION` is enabled, `sm_init()` should also be called in dual-mode or Classic-oriented examples that want CTKD support. This allows BTstack to derive and store transport keys across LE and BR/EDR after Secure Connections pairing.

The examples in `example/` already show the relevant building blocks:

- `example/spp_counter.c` shows a basic Classic service with legacy PIN rejection and SSP handling.
- `example/gap_dedicated_bonding.c` shows explicit dedicated bonding.
- `example/hid_keyboard_demo.c` shows a profile that relies on pairing and bonding in practice.

### Security Levels and Their Meaning

BTstack exposes the Classic security target via `gap_set_security_level()`:

- `LEVEL_0`: no security
- `LEVEL_1`: no encryption, minimal user interaction
- `LEVEL_2`: encryption required, no MITM protection
- `LEVEL_3`: encryption and MITM protection required
- `LEVEL_4`: encryption, MITM protection, and Secure Connections strength required

For most Classic examples, these are the practical choices:

- use `LEVEL_2` for standard encrypted pairing
- use `LEVEL_3` if the remote user must confirm or enter a passkey
- use `LEVEL_4` if only Secure Connections grade pairing is acceptable

### Example 1: Use the Default Classic Security Setup

The `spp_counter` example already uses the default Mode 4 behavior. The minimal setup is:

    static void btstack_main_security_setup(void){
        l2cap_init();

    #ifdef ENABLE_BLE
        // Needed if Cross-Transport Key Derivation is enabled
        sm_init();
    #endif

        gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
        gap_set_local_name("SPP Counter 00:00:00:00:00:00");
        gap_discoverable_control(1);
    }

With this configuration, BTstack keeps Security Mode 4 and `LEVEL_2`. If the remote device is old, it may still request legacy PIN pairing. The examples reject this request instead of silently accepting a fixed PIN. Secure Simple Pairing was introduced with the Bluetooth 2.1 specification in 2007, so Bluetooth 2.1 and newer devices use SSP instead of legacy PIN pairing.

The application should handle both cases in the packet handler:

    static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
        UNUSED(channel);
        UNUSED(size);

        bd_addr_t event_addr;

        if (packet_type != HCI_EVENT_PACKET) return;

        switch (hci_event_packet_get_type(packet)) {
            case HCI_EVENT_PIN_CODE_REQUEST:
                printf("Pin code request for Legacy Pairing received -> abort pairing\n");
                hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                gap_pin_code_negative(event_addr);
                break;

            case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n",
                       hci_event_user_confirmation_request_get_numeric_value(packet));
                printf("Accepting Pairing - TODO: require actual user action\n");
                hci_event_user_confirmation_request_get_bd_addr(packet, event_addr);
                gap_ssp_confirmation_response(event_addr);
                break;

            default:
                break;
        }
    }

This is the right baseline for examples like SPP that need encrypted Classic connections but do not need stronger authentication than standard SSP. Legacy PIN pairing is intentionally not accepted by default; an application that really needs to support pre-Bluetooth 2.1 devices must make that policy decision explicitly and provide its own PIN handling.

If CTKD is enabled, this baseline is also sufficient to allow an LE Secure Connections bond to later create a usable Classic link key, as long as the LE Security Manager is initialized and bonding data is persisted.

### Example 2: Require MITM Protection for a Classic Service

If a service should require authenticated pairing, set the security level before the profile or service is initialized:

    static void classic_security_setup_mitm(void){
        gap_set_security_level(LEVEL_3);
        gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
        gap_ssp_set_authentication_requirement(
            SSP_IO_AUTHREQ_MITM_PROTECTION_REQUIRED_GENERAL_BONDING);
    }

Important points:

- `gap_set_security_level()` must be called before BTstack registers Classic services that depend on `gap_get_security_level()`.
- `LEVEL_3` requires MITM protection, so `SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT` is usually not appropriate.
- The actual SSP association model depends on both devices' IO capabilities.

Typical event handling then becomes:

    case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        hci_event_user_confirmation_request_get_bd_addr(packet, event_addr);
        printf("Numeric comparison %06"PRIu32"\n",
               hci_event_user_confirmation_request_get_numeric_value(packet));
        gap_ssp_confirmation_response(event_addr);
        break;

    case HCI_EVENT_USER_PASSKEY_REQUEST:
        hci_event_user_passkey_request_get_bd_addr(packet, event_addr);
        gap_ssp_passkey_response(event_addr, 123456);
        break;

This setup is appropriate for Classic HID, control channels, or administrative services where "Just Works" pairing is not sufficient.

### Example 3: Force Secure Connections Quality

If the application should only accept Secure Connections grade authentication for Classic pairing, use `LEVEL_4`. On top of that, BTstack can enforce Secure Connections Only mode:

    static void classic_security_setup_secure_connections_only(void){
        gap_set_security_level(LEVEL_4);
        gap_set_secure_connections_only_mode(true);
        gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
        gap_ssp_set_authentication_requirement(
            SSP_IO_AUTHREQ_MITM_PROTECTION_REQUIRED_GENERAL_BONDING);
    }

Use this only when all intended peers support Secure Connections. Legacy peers will no longer be able to pair successfully.

### Example 4: Start Dedicated Bonding Explicitly

If bonding should happen before any profile connection is opened, use dedicated bonding as shown in `gap_dedicated_bonding.c`. In the current example, the application also handles SSP numeric confirmation explicitly:

    static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
        UNUSED(channel);
        UNUSED(size);

        if (packet_type != HCI_EVENT_PACKET) return;

        switch (hci_event_packet_get_type(packet)) {
            case BTSTACK_EVENT_STATE:
                if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                    gap_dedicated_bonding(device_addr, 1);
                }
                break;
            case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                hci_event_user_confirmation_request_get_bd_addr(packet, event_addr);
                printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n",
                       hci_event_user_confirmation_request_get_numeric_value(packet));
                gap_ssp_confirmation_response(event_addr);
                break;
            case GAP_EVENT_DEDICATED_BONDING_COMPLETED:
                printf("Dedicated bonding completed, status 0x%02x\n",
                       gap_event_dedicated_bonding_completed_get_status(packet));
                break;
            default:
                break;
        }
    }

This is useful if the user interface has an explicit "pair device" action and the product should finish bonding before opening RFCOMM, HID, or other profile channels.

### Example 5: Raise Security for a Specific Existing Connection

If a connection already exists and the application now needs stronger protection, request it explicitly:

    gap_request_security_level(con_handle, LEVEL_3);

BTstack reports the result with `GAP_EVENT_SECURITY_LEVEL`. This is useful for outgoing Classic client roles that do not want to require authentication immediately after link setup.

### Notes on Bonding Persistence

Pairing is only useful across reboots if link keys are stored persistently.

- On Classic, BTstack stores a link key plus its type.
- If link keys are not persisted, the devices will have to pair again after reset.
- The number of stored Classic bonds is limited by `NVM_NUM_LINK_KEYS`.

On POSIX ports, persistent link key storage is typically provided by `platform/posix/btstack_link_key_db_fs.c`. On embedded targets, link keys are commonly stored in TLV-backed flash storage.

For CTKD, persistence on both sides matters:

- the Classic link key database must be available to retain derived BR/EDR keys
- the LE device database must be available to retain derived LE keys

If one side is not persisted, the corresponding CTKD-derived trust relationship will be lost after reset even if the other side is still stored.

## GATT over Classic Bluetooth Security Setup in Examples {#sec:gattOverClassicSecurityExamples}

When `ENABLE_GATT_OVER_CLASSIC` is enabled, BTstack can expose the same GATT database over BR/EDR and LE at the same time.

From the application perspective, this creates two independent security domains:

- Classic GATT security is controlled by Classic GAP and SSP configuration.
- LE GATT security is controlled by the LE Security Manager configuration.

This is why the dual-mode GATT examples contain both Classic security calls and LE security calls in the same setup function.

The main examples to look at are:

- `example/gatt_counter.c`
- `example/gatt_streamer_server.c`
- `example/spp_and_gatt_counter.c`
- `example/spp_and_gatt_streamer.c`

### What Is Secured by Which Stack Component

For GATT over Classic:

- discovery of the GATT service happens via Classic SDP
- transport uses Classic L2CAP ATT
- pairing and bonding use Classic GAP / SSP
- link security level is controlled via `gap_set_security_level()` and related GAP APIs

For GATT over LE:

- discovery happens via LE advertisements and GATT procedures
- transport uses LE ATT
- pairing and bonding use the LE Security Manager
- security requirements are configured with `sm_set_io_capabilities()` and `sm_set_authentication_requirements()`

BTstack keeps the ATT database and ATT callbacks shared, but the security procedures are transport-specific.

If `ENABLE_CROSS_TRANSPORT_KEY_DERIVATION` is enabled, the two security domains can still benefit from each other after Secure Connections bonding, but they are not merged. CTKD derives keys across transports; it does not replace Classic GAP/SSP policy or LE Security Manager policy.

### Example 1: Default Dual-Mode GATT Server Security

The `gatt_counter` and `gatt_streamer_server` examples use the simplest dual-mode security split:

- LE side: Just Works, no bonding
- Classic side: normal SSP configuration for BR/EDR reachability

The setup looks like this:

    static void gatt_counter_setup(void){
        l2cap_init();

        // LE security setup
        sm_init();
        sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
        sm_set_authentication_requirements(0);

    #ifdef ENABLE_GATT_OVER_CLASSIC
        // publish GATT service over Classic
        sdp_init();
        memset(gatt_service_buffer, 0, sizeof(gatt_service_buffer));
        gatt_create_sdp_record(gatt_service_buffer,
                               sdp_create_service_record_handle(),
                               ATT_SERVICE_GATT_SERVICE_START_HANDLE,
                               ATT_SERVICE_GATT_SERVICE_END_HANDLE);
        sdp_register_service(gatt_service_buffer);

        // Classic SSP setup
        gap_set_local_name("GATT Counter BR/EDR 00:00:00:00:00:00");
        gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
        gap_discoverable_control(1);
    #endif

        att_server_init(profile_data, att_read_callback, att_write_callback);
    }

This is the current baseline if you want one GATT server that is reachable via both transports without requiring authenticated pairing.

If CTKD is enabled, this baseline also allows a successful LE Secure Connections pairing to seed a future Classic GATT connection with derived BR/EDR bonding material.

### Example 2: Require Stronger Security for Classic GATT

If the BR/EDR side of the GATT service should require encrypted or authenticated Classic links, configure the Classic security level before the GATT service is exposed:

    static void gatt_over_classic_security_setup(void){
        gap_set_security_level(LEVEL_3);
        gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
        gap_ssp_set_authentication_requirement(
            SSP_IO_AUTHREQ_MITM_PROTECTION_REQUIRED_GENERAL_BONDING);
    }

Important points:

- `gap_set_security_level()` affects the Classic ATT bearer in the same way it affects other Classic services.
- `LEVEL_2` requires encryption but not MITM protection.
- `LEVEL_3` requires MITM protection and is the practical choice if the Classic GATT service carries sensitive control or configuration data.
- `LEVEL_4` can be used if Secure Connections grade protection is required for Classic GATT as well.

This configuration should be applied before `att_server_init()` registers the Classic ATT service and before the SDP record is published.

### Example 3: Keep LE and Classic Security Policies Different

In many products, LE and Classic do not need the same policy.

For example:

- LE side may use `sm_set_authentication_requirements(0)` for simple app access.
- Classic side may use `gap_set_security_level(LEVEL_3)` because the BR/EDR GATT service exposes administrative features.

This is a valid BTstack setup because the two transports negotiate security independently, even though they share the same ATT database.

A mixed setup could look like this:

    static void dual_mode_gatt_security_setup(void){
        // LE: Just Works, no bonding
        sm_init();
        sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
        sm_set_authentication_requirements(0);

    #ifdef ENABLE_GATT_OVER_CLASSIC
        // Classic: authenticated and bondable
        gap_set_security_level(LEVEL_3);
        gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
        gap_ssp_set_authentication_requirement(
            SSP_IO_AUTHREQ_MITM_PROTECTION_REQUIRED_GENERAL_BONDING);
    #endif
    }

This is often the most practical way to think about security in dual-mode GATT examples: one shared application protocol, two transport-specific security policies.

### Example 4: Characteristic Permissions and Transport Security

The GATT database itself can also require security by marking characteristics with flags such as:

- `READ_ENCRYPTED`
- `READ_AUTHENTICATED`
- `WRITE_ENCRYPTED`
- `WRITE_AUTHENTICATED`
- `AUTHENTICATION_REQUIRED`

For dual-mode GATT servers, these permissions are evaluated against the current transport security state:

- on LE, against LE pairing / encryption state
- on Classic, against BR/EDR link security state

This means that the same `.gatt` file can enforce protected reads and writes over both transports, while BTstack maps the check onto the appropriate underlying security mechanism.

### Example 5: Event Handling in Dual-Mode GATT Servers

A dual-mode GATT server often needs to handle both:

- LE security events such as `SM_EVENT_JUST_WORKS_REQUEST`
- Classic pairing events such as `HCI_EVENT_USER_CONFIRMATION_REQUEST`

For example, the LE side of `gatt_counter.c` confirms Just Works pairing:

    case SM_EVENT_JUST_WORKS_REQUEST:
        sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
        break;

If the application also wants explicit control over Classic SSP, it should additionally handle:

    case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        hci_event_user_confirmation_request_get_bd_addr(packet, event_addr);
        gap_ssp_confirmation_response(event_addr);
        break;

Without `gap_ssp_set_auto_accept(1)`, BTstack expects the application to answer Classic SSP confirmation or passkey requests itself.

### Choosing an Example

Start from these examples depending on the required security shape:

- `gatt_counter.c` for the smallest dual-mode GATT server baseline.
- `gatt_streamer_server.c` for a higher-throughput dual-mode GATT server.
- `spp_and_gatt_counter.c` if Classic SPP and dual-mode GATT should coexist.
- `spp_and_gatt_streamer.c` if throughput on both SPP and GATT matters.

The main rule is:

- configure LE security with Security Manager APIs
- configure Classic GATT security with GAP / SSP APIs
- use `.gatt` permissions to express which GATT attributes actually require protected access

If CTKD is part of the product design, add these two checks:

- initialize the LE Security Manager with `sm_init()` even if the visible product flow is primarily Classic
- verify CTKD activity during testing via `SM_EVENT_PAIRING_COMPLETE` and `sm_event_pairing_complete_get_ctkd_active(packet)`

## LE Credit-Based Flow Control Mode Security Setup in Examples {#sec:leCbmSecurityExamples}

BTstack implements LE Credit-Based Flow Control Mode with the `l2cap_cbm_*` API family.

From the security perspective, LE CBM is simpler than dual-mode GATT:

- it always uses LE transport security
- incoming channel policy is declared when registering the local service
- outgoing channel policy is declared when creating the channel

The current examples to look at are:

- `example/le_credit_based_flow_control_mode_server.c`
- `example/le_credit_based_flow_control_mode_client.c`

### Incoming LE CBM Security

For incoming LE CBM channels, the minimum required security level is configured at service registration time:

    l2cap_cbm_register_service(packet_handler, psm, LEVEL_2);

This means:

- `LEVEL_0` allows an unencrypted LE link
- `LEVEL_2` requires encryption
- `LEVEL_3` requires authenticated pairing
- `LEVEL_4` requires Secure Connections grade protection

If a remote device tries to open an LE CBM channel and the current LE link security is too low, BTstack rejects the connection request with the corresponding L2CAP result code.

The server example currently uses `LEVEL_2`, so the client must establish an encrypted LE link before the channel can be opened successfully.

### Outgoing LE CBM Security

For outgoing LE CBM channels, the minimum required security level is passed directly to `l2cap_cbm_create_channel(...)`:

    l2cap_cbm_create_channel(&packet_handler, connection_handle, psm,
                             receive_buffer, sizeof(receive_buffer),
                             L2CAP_LE_AUTOMATIC_CREDITS, LEVEL_2, &local_cid);

This is the client-side equivalent of `l2cap_cbm_register_service(...)`.

The LE CBM client example also uses `LEVEL_2`, which means it requests an encrypted LE connection before the channel is considered acceptable.

### Security Manager Setup

Because LE CBM relies on LE link security, applications that require any protected LE CBM service should initialize the LE Security Manager:

    sm_init();

This is the baseline requirement if:

- the LE CBM service uses `LEVEL_2` or higher
- the LE CBM client requests `LEVEL_2` or higher
- the product should pair or bond automatically during connection setup

Without `sm_init()`, BTstack cannot manage LE pairing for the protected LE CBM channel.

### Pairing and Bonding Policy

The chosen LE bonding and authentication policy still comes from the normal Security Manager configuration:

    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(0);

or, for stronger protection:

    sm_set_authentication_requirements(SM_AUTHREQ_MITM_PROTECTION | SM_AUTHREQ_BONDING);

In other words:

- `l2cap_cbm_register_service(..., LEVEL_X)` and `l2cap_cbm_create_channel(..., LEVEL_X, ...)` say how strong the link must be
- `sm_set_authentication_requirements(...)` says how LE pairing should be performed when BTstack raises the link to that level

### Practical Example

The examples currently use this practical combination:

- LE CBM channel requires `LEVEL_2`
- LE Security Manager is initialized
- Simple LE pairing is sufficient for the example throughput test

This is a good default for products that need protected bulk transfer over LE without requiring user interaction.

If the transferred data is sensitive, raise the LE CBM security level and adjust the Security Manager policy accordingly:

- use `LEVEL_3` for authenticated pairing
- use `LEVEL_4` if Secure Connections strength is required
- configure `sm_set_authentication_requirements(...)` to match the desired pairing method

### Main Rule

For LE Credit-Based Flow Control Mode:

- set the required link security with the `security_level` argument on service registration or channel creation
- initialize and configure the LE Security Manager whenever the required level is above `LEVEL_0`
- treat LE CBM security as LE link security, not as application-layer security

## Bluetooth Power Control {#sec:powerControl}

In most BTstack examples, the device is set to be discoverable and connectable. In this mode, even when there's no active connection, the Bluetooth Controller will periodically activate its receiver in order to listen for inquiries or connecting requests from another device.
The ability to be discoverable requires more energy than the ability to be connected. Being discoverable also announces the device to anybody in the area. Therefore, it is a good idea to pause listening for inquiries when not needed. Other devices that have your Bluetooth address can still connect to your device.

To enable/disable discoverability, you can call:

    /**
     * @brief Allows to control if device is discoverable. OFF by default.
     */
    void gap_discoverable_control(uint8_t enable);

If you don't need to become connected from other devices for a longer period of time, you can also disable the listening to connection requests.

To enable/disable connectability, you can call:

    /**
     * @brief Override page scan mode. Page scan mode enabled by l2cap when services are registered
     * @note Might be used to reduce power consumption while Bluetooth module stays powered but no (new)
     *       connections are expected
     */
    void gap_connectable_control(uint8_t enable);

For Bluetooth Low Energy, the radio is periodically used to broadcast advertisements that are used for both discovery and connection establishment.

To enable/disable advertisements, you can call:

    /**
     * @brief Enable/Disable Advertisements. OFF by default.
     * @param enabled
     */
    void gap_advertisements_enable(int enabled);

If a Bluetooth Controller is neither discoverable nor connectable, it does not need to periodically turn on its radio and it only needs to respond to commands from the Host. In this case, the Bluetooth Controller is free to enter some kind of deep sleep where the power consumption is minimal.

Finally, if that's not sufficient for your application, you could request BTstack to shutdown the Bluetooth Controller. For this, the "on" and "off" functions in the btstack_control_t struct must be implemented. To shutdown the Bluetooth Controller, you can call:

    /**
     * @brief Requests the change of BTstack power mode.
     */
    int  hci_power_control(HCI_POWER_MODE mode);

with mode set to *HCI_POWER_OFF*. When needed later, Bluetooth can be started again via by calling it with mode *HCI_POWER_ON*, as seen in all examples.
