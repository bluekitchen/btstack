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

\#define | Description
-----------------------------------|-------------------------------------
HAVE_MALLOC                        | Use dynamic memory
HAVE_AES128                        | Use platform AES128 engine - not needed usually
HAVE_BTSTACK_STDIN                 | STDIN is available for CLI interface
HAVE_MBEDTLS_ECC_P256              | mbedTLS provides NIST P-256 operations e.g. for LE Secure Connections

Embedded platform properties:

\#define                           | Description
-----------------------------------|------------------------------------
HAVE_EMBEDDED_TIME_MS              | System provides time in milliseconds
HAVE_EMBEDDED_TICK                 | System provides tick interrupt

FreeRTOS platform properties:

\#define                           | Description
-----------------------------------|------------------------------------
HAVE_FREERTOS_INCLUDE_PREFIX       | FreeRTOS headers are in 'freertos' folder (e.g. ESP32's esp-idf)

POSIX platform properties:

\#define                            | Description
-----------------------------------|------------------------------------
HAVE_POSIX_B300_MAPPED_TO_2000000  | Workaround to use serial port with 2 mbps
HAVE_POSIX_B600_MAPPED_TO_3000000  | Workaround to use serial port with 3 mpbs
HAVE_POSIX_FILE_IO                 | POSIX File i/o used for hci dump
HAVE_POSIX_TIME                    | System provides time function
LINK_KEY_PATH                      | Path to stored link keys
LE_DEVICE_DB_PATH                  | Path to stored LE device information
<!-- a name "lst:btstackFeatureConfiguration"></a-->
<!-- -->

### ENABLE_* directives {#sec:enableDirectives}
BTstack properties:

\#define                         | Description
---------------------------------|---------------------------------------------
ENABLE_CLASSIC                   | Enable Classic related code in HCI and L2CAP
ENABLE_BLE                       | Enable BLE related code in HCI and L2CAP
ENABLE_EHCILL                    | Enable eHCILL low power mode on TI CC256x/WL18xx chipsets
ENABLE_H5                        | Enable support for SLIP mode in `btstack_uart.h` drivers for HCI H5 ('Three-Wire Mode')
ENABLE_LOG_DEBUG                 | Enable log_debug messages
ENABLE_LOG_ERROR                 | Enable log_error messages
ENABLE_LOG_INFO                  | Enable log_info messages
ENABLE_SCO_OVER_HCI              | Enable SCO over HCI for chipsets (if supported)
ENABLE_SCO_OVER_PCM              | Enable SCO ofer PCM/I2S for chipsets (if supported)
ENABLE_HFP_WIDE_BAND_SPEECH      | Enable support for mSBC codec used in HFP profile for Wide-Band Speech
ENABLE_HFP_AT_MESSAGES           | Enable `HFP_SUBEVENT_AT_MESSAGE_SENT` and `HFP_SUBEVENT_AT_MESSAGE_RECEIVED` events
ENABLE_LE_PERIPHERAL             | Enable support for LE Peripheral Role in HCI and Security Manager
ENBALE_LE_CENTRAL                | Enable support for LE Central Role in HCI and Security Manager
ENABLE_LE_SECURE_CONNECTIONS     | Enable LE Secure Connections
ENABLE_LE_PROACTIVE_AUTHENTICATION | Enable automatic encryption for bonded devices on re-connect
ENABLE_GATT_CLIENT_PAIRING       | Enable GATT Client to start pairing and retry operation on security error
ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS | Use [micro-ecc library](https://github.com/kmackay/micro-ecc) for ECC operations
ENABLE_LE_DATA_CHANNELS          | Enable LE Data Channels in credit-based flow control mode
ENABLE_LE_DATA_LENGTH_EXTENSION  | Enable LE Data Length Extension support
ENABLE_LE_SIGNED_WRITE           | Enable LE Signed Writes in ATT/GATT
ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION | Enable address resolution for resolvable private addresses in Controller
ENABLE_CROSS_TRANSPORT_KEY_DERIVATION | Enable Cross-Transport Key Derivation (CTKD) for Secure Connections
ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE | Enable L2CAP Enhanced Retransmission Mode. Mandatory for AVRCP Browsing
ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL | Enable HCI Controller to Host Flow Control, see below
ENABLE_ATT_DELAYED_RESPONSE      | Enable support for delayed ATT operations, see [GATT Server](profiles/#sec:GATTServerProfile)
ENABLE_BCM_PCM_WBS               | Enable support for Wide-Band Speech codec in BCM controller, requires ENABLE_SCO_OVER_PCM
ENABLE_CC256X_ASSISTED_HFP       | Enable support for Assisted HFP mode in CC256x Controller, requires ENABLE_SCO_OVER_PCM
ENABLE_CC256X_BAUDRATE_CHANGE_FLOWCONTROL_BUG_WORKAROUND | Enable workaround for bug in CC256x Flow Control during baud rate change, see chipset docs.
ENABLE_CYPRESS_BAUDRATE_CHANGE_FLOWCONTROL_BUG_WORKAROUND | Enable workaround for bug in CYW2070x Flow Control during baud rate change, similar to CC256x.
ENABLE_LE_LIMIT_ACL_FRAGMENT_BY_MAX_OCTETS | Force HCI to fragment ACL-LE packets to fit into over-the-air packet
ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD | Enable use of explicit delete field in TLV Flash implemenation - required when flash value cannot be overwritten with zero
ENABLE_CONTROLLER_WARM_BOOT      | Enable stack startup without power cycle (if supported/possible)
ENABLE_SEGGER_RTT                | Use SEGGER RTT for console output and packet log, see [additional options](#sec:rttConfiguration)
ENABLE_EXPLICIT_CONNECTABLE_MODE_CONTROL | Disable calls to control Connectable Mode by L2CAP
ENABLE_EXPLICIT_IO_CAPABILITIES_REPLY | Let application trigger sending IO Capabilities (Negative) Reply
ENABLE_CLASSIC_OOB_PAIRING       | Enable support for classic Out-of-Band (OOB) pairing
ENABLE_A2DP_SOURCE_EXPLICIT_CONFIG | Let application configure stream endpoint (skip auto-config of SBC endpoint)

Notes:

- ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS: Only some Bluetooth 4.2+ controllers (e.g., EM9304, ESP32) support the necessary HCI commands for ECC. Other reason to enable the ECC software implementations are if the Host is much faster or if the micro-ecc library is already provided (e.g., ESP32, WICED, or if the ECC HCI Commands are unreliable.

### HCI Controller to Host Flow Control
In general, BTstack relies on flow control of the HCI transport, either via Hardware CTS/RTS flow control for UART or regular USB flow control. If this is not possible, e.g on an SoC, BTstack can use HCI Controller to Host Flow Control by defining ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL. If enabled, the HCI Transport implementation must be able to buffer the specified packets. In addition, it also need to be able to buffer a few HCI Events. Using a low number of host buffers might result in less throughput.

Host buffer configuration for HCI Controller to Host Flow Control:

\#define         | Description
------------------|------------
HCI_HOST_ACL_PACKET_NUM | Max number of ACL packets
HCI_HOST_ACL_PACKET_LEN | Max size of HCI Host ACL packets
HCI_HOST_SCO_PACKET_NUM | Max number of ACL packets
HCI_HOST_SCO_PACKET_LEN | Max size of HCI Host SCO packets


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

For each HCI connection, a buffer of size HCI_ACL_PAYLOAD_SIZE is reserved. For fast data transfer, however, a large ACL buffer of 1021 bytes is recommend. The large ACL buffer is required for 3-DH5 packets to be used.

<!-- a name "lst:memoryConfiguration"></a-->
<!-- -->

\#define | Description
--------|------------
HCI_ACL_PAYLOAD_SIZE | Max size of HCI ACL payloads
MAX_NR_BNEP_CHANNELS | Max number of BNEP channels
MAX_NR_BNEP_SERVICES | Max number of BNEP services
MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES | Max number of link key entries cached in RAM
MAX_NR_GATT_CLIENTS | Max number of GATT clients
MAX_NR_HCI_CONNECTIONS | Max number of HCI connections
MAX_NR_HFP_CONNECTIONS | Max number of HFP connections
MAX_NR_L2CAP_CHANNELS |  Max number of L2CAP connections
MAX_NR_L2CAP_SERVICES |  Max number of L2CAP services
MAX_NR_RFCOMM_CHANNELS | Max number of RFOMMM connections
MAX_NR_RFCOMM_MULTIPLEXERS | Max number of RFCOMM multiplexers, with one multiplexer per HCI connection
MAX_NR_RFCOMM_SERVICES | Max number of RFCOMM services
MAX_NR_SERVICE_RECORD_ITEMS | Max number of SDP service records
MAX_NR_SM_LOOKUP_ENTRIES | Max number of items in Security Manager lookup queue
MAX_NR_WHITELIST_ENTRIES | Max number of items in GAP LE Whitelist to connect to
MAX_NR_LE_DEVICE_DB_ENTRIES | Max number of items in LE Device DB


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
    #define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES  3

Listing: Memory configuration for a basic SPP server. {#lst:memoryConfigurationSPP}

In this example, the size of ACL packets is limited to the minimum of 52 bytes, resulting in an L2CAP MTU of 48 bytes. Only a singleHCI connection can be established at any time. On it, two L2CAP services are provided, which can be active at the same time. Here, these two can be RFCOMM and SDP. Then, memory for one RFCOMM multiplexer is reserved over which one connection can be active. Finally, up to three link keys can be cached in RAM.

<!-- -->

### Non-volatile memory (NVM) directives {#sec:nvmConfiguration}

If implemented, bonding information is stored in Non-volatile memory. For Classic, a single link keys and its type is stored. For LE, the bonding information contains various values (long term key, random number, EDIV, signing counter, identity, ...) Often, this is implemented using Flash memory. Then, the number of stored entries are limited by:

<!-- a name "lst:nvmDefines"></a-->
<!-- -->

\#define                  | Description
--------------------------|------------
NVM_NUM_LINK_KEYS         | Max number of Classic Link Keys that can be stored 
NVM_NUM_DEVICE_DB_ENTRIES | Max number of LE Device DB entries that can be stored
NVN_NUM_GATT_SERVER_CCC   | Max number of 'Client Characteristic Configuration' values that can be stored by GATT Server


### SEGGER Real Time Transfer (RTT) directives {#sec:rttConfiguration}

[SEGGER RTT](https://www.segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/) improves on the use of an UART for debugging with higher throughput and less overhead. In addition, it allows for direct logging in PacketLogger/BlueZ format via the provided JLinkRTTLogger tool.

When enabled with `ENABLE_SEGGER_RTT` and `hci_dump_init()` can be called with an `hci_dunp_segger_stdout_get_instance()` for textual output and `hci_dump_segger_binary_get_instance()` for binary output. With the latter, you can select `HCI_DUMP_BLUEZ` or `HCI_DUMP_PACKETLOGGER`, format. For RTT, the following directives are used to configure the up channel:

\#define                         | Default                        | Description
---------------------------------|--------------------------------|------------------------
SEGGER_RTT_PACKETLOG_MODE        | SEGGER_RTT_MODE_NO_BLOCK_SKIP  | SEGGER_RTT_MODE_NO_BLOCK_SKIP to skip messages if buffer is full, or, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL to block 
SEGGER_RTT_PACKETLOG_CHANNEL     | 1                              | Channel to use for packet log. Channel 0 is used for terminal
SEGGER_RTT_PACKETLOG_BUFFER_SIZE | 1024                           | Size of outgoing ring buffer. Increase if you cannot block but get 'message skipped' warnings.

## Run-time configuration

To allow code-reuse with different platforms
as well as with new ports, the low-level initialization of BTstack and
the hardware configuration has been extracted to the various
*platforms/PLATFORM/main.c* files. The examples only contain the
platform-independent Bluetooth logic. But let’s have a look at the
common init code.

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

First, BTstack’s memory pools are setup up. Then, the standard run loop
implementation for embedded systems is selected.

The call to *hci_dump_init* configures BTstack to output all Bluetooth
packets and its own debug and error message using printf with BTstack's
millisecond tiomestamps.s as tim.
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

Path                | Description
--------------------|---------------
chipset             | Support for individual Bluetooth Controller chipsets
doc                 | Sources for BTstack documentation
example             | Example applications available for all ports
platform            | Support for special OSs and/or MCU architectures
port                | Complete port for a MCU + Chipset combinations
src                 | Bluetooth stack implementation
test                | Unit and PTS tests
tool                | Helper tools for BTstack

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
periodic events.

Data sources and timers are represented by the *btstack_data_source_t* and
*btstack_timer_source_t* structs respectively. Each of these structs contain
at least a linked list node and a pointer to a callback function. All active timers
and data sources are kept in link lists. While the list of data sources
is unsorted, the timers are sorted by expiration timeout for efficient
processing.

Timers are single shot: a timer will be removed from the timer list
before its event handler callback is executed. If you need a periodic
timer, you can re-register the same timer source in the callback
function, as shown in Listing [PeriodicTimerHandler]. Note that BTstack
expects to get called periodically to keep its time, see Section
[on time abstraction](#sec:timeAbstractionPorting) for more on the
tick hardware abstraction.

BTstack provides different run loop implementations that implement the *btstack_run_loop_t* interface:

- Embedded: the main implementation for embedded systems, especially without an RTOS.
- FreeRTOS: implementation to run BTstack on a dedicated FreeRTOS thread
- POSIX: implementation for POSIX systems based on the select() call.
- CoreFoundation: implementation for iOS and OS X applications
- WICED: implementation for the Broadcom WICED SDK RTOS abstraction that wraps FreeRTOS or ThreadX.
- Windows: implementation for Windows based on Event objects and WaitForMultipleObjects() call.

Depending on the platform, data sources are either polled (embedded, FreeRTOS), or the platform provides a way
to wait for a data source to become ready for read or write (POSIX, CoreFoundation, Windows), or,
are not used as the HCI transport driver and the run loop is implemented in a different way (WICED).
In any case, the callbacks must be to explicitly enabled with the *btstack_run_loop_enable_data_source_callbacks(..)* function.

In your code, you'll have to configure the run loop before you start it
as shown in Listing [listing:btstackInit]. The application can register
data sources as well as timers, e.g., for periodical sampling of sensors, or
for communication over the UART.

The run loop is set up by calling *btstack_run_loop_init* function and providing
an instance of the actual run loop. E.g. for the embedded platform, it is:

<!-- -->

    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

The complete Run loop API is provided [here](appendix/apis/#sec:runLoopAPIAppendix).

### Run loop embedded

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
*btstack_run_loop_embedded_trigger* function. The call to
*btstack_run_loop_embedded_trigger* sets an
internal flag that is checked in the critical section just before
entering sleep mode causing another run loop cycle.

To enable the use of timers, make sure that you defined HAVE_EMBEDDED_TICK or HAVE_EMBEDDED_TIME_MS in the
config file.

### Run loop FreeRTOS

The FreeRTOS run loop is used on a dedicated FreeRTOS thread and it uses a FreeRTOS queue to schedule callbacks on the run loop.
In each iteration:

- all data sources are polled
- all scheduled callbacks are executed
- all expired timers are called
- finally, it gets the next timeout. It then waits for a 'trigger' or the next timeout, if set.

To trigger the run loop, *btstack_run_loop_freertos_trigger* and *btstack_run_loop_freertos_trigger_from_isr* can be called.
This causes the data sources to get polled.

Alternatively. *btstack_run_loop_freertos_execute_code_on_main_thread* can be used to schedule a callback on the main loop.
Please note that the queue is finite (see *RUN_LOOP_QUEUE_LENGTH* in btstack_run_loop_embedded).

### Run loop POSIX

The data sources are standard File Descriptors. In the run loop execute implementation,
select() call is used to wait for file descriptors to become ready to read or write,
while waiting for the next timeout.

To enable the use of timers, make sure that you defined HAVE_POSIX_TIME in the config file.

### Run loop CoreFoundation (OS X/iOS)

This run loop directly maps BTstack's data source and timer source with CoreFoundation objects.
It supports ready to read and write similar to the POSIX implementation. The call to
*btstack_run_loop_execute()* then just calls *CFRunLoopRun()*.

To enable the use of timers, make sure that you defined HAVE_POSIX_TIME in the config file.

### Run loop Windows

The data sources are Event objects. In the run loop implementation WaitForMultipleObjects() call
is all is used to wait for the Event object to become ready while waiting for the next timeout.


### Run loop WICED

WICED SDK API does not provide asynchronous read and write to the UART and no direct way to wait for
one or more peripherals to become ready. Therefore, BTstack does not provide direct support for data sources.
Instead, the run loop provides a message queue that allows to schedule functions calls on its thread via
*btstack_run_loop_wiced_execute_code_on_main_thread()*.

The HCI transport H4 implementation then uses two lightweight threads to do the
blocking read and write operations. When a read or write is complete on
the helper threads, a callback to BTstack is scheduled.


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


Packet Handler                 | Registering Function
-------------------------------|--------------------------------------
HCI packet handler             | hci_add_event_handler
L2CAP packet handler           | l2cap_register_packet_handler
L2CAP service packet handler   | l2cap_register_service
L2CAP channel packet handler   | l2cap_create_channel
RFCOMM service packet handler  | rfcomm_register_service and rfcomm_register_service_with_initial_credits
RFCOMM channel packet handler  | rfcomm_create_channel and rfcomm_create_channel_with_initial_credits


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

Platform | File                         | Description
---------|------------------------------|------------
POSIX    | `hci_dump_posix_fs.c`        | HCI log file for Apple PacketLogger and Wireshark
POSIX    | `hci_dump_posix_stdout.c`    | Console output via printf
Embedded | `hci_dump_embedded_stdout.c` | Console output via printf
Embedded | `hci_dump_segger_stdout.c`   | Console output via SEGGER RTT
Embedded | `hci_dump_segger_binary.c`   | HCI log file for Apple PacketLogger via SEGGER RTT


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
