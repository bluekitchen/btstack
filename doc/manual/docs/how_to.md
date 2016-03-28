
BTstack implements a set of Bluetooth protocols and profiles. To connect to other devices or to provide a Bluetooth services, BTstack has to be properly configured.

The configuration of BTstack is done both at compile time as well as at run time:

- *btstack_config.h* to describe the system configuration, used functionality, and also the memory configuration
- adding necessary source code files to your project
- run time configuration of provided services during startup

In the following, we provide an overview of the the configuration via *btstack_config.h*, the memory management, the
run loop, and services that are necessary to setup BTstack. From the
point when the run loop is executed, the application runs as a finite
state machine, which processes events received from BTstack. BTstack
groups events logically and provides them over packet handlers, of which
an overview is provided here. Finally, we describe the RFCOMM
credit-based flow-control, which may be necessary for
resource-constraint devices.

## Configuration in btstack_config.h {#sec:btstackConfigHowTo}
The file *btstack_config.h* contains three parts:

- description of available system features, similar to config.h in a autoconf setup. All #defines start with HAVE_ and are [listed here](#sec:platformConfiguration)
- list of enabled features, most importantly ENABLE_CLASSIC and ENABLE_BLE. [See here](#sec:btstackFeatureConfiguration)
- other BTstack configuration, most notably static memory. [See next section](#sec:memoryConfigurationHowTo).

<!-- a name "lst:platformConfiguration"></a-->
<!-- -->

#define | Platform | Explanation
-----------------------------|-------|------------------------------------
HAVE_B1200_MAPPED_TO_2000000 | posix | Hack to use serial port with 2 mbps 
HAVE_B2400_MAPPED_TO_3000000 | posix | Hack to use serial port with 3 mbps
HAVE_B300_MAPPED_TO_2000000  | posix | Hack to use serial port with 2 mbps
HAVE_B600_MAPPED_TO_3000000  | posix | Hack to use serial port with 3 mpbs
HAVE_EHCILL                  | cc256x radio | CC256x/WL18xx with eHCILL is used
HAVE_MALLOC                  |       | dynamic memory used
HAVE_POSIX_FILE_IO           | posix | POSIX File i/o used for hci dump
HAVE_SO_NOSIGPIPE            | posix | libc supports SO_NOSIGPIPE 
HAVE_STDIO                   |       | STDIN is available for examples
HAVE_TICK                    | embedded | System provides tick interrupt
HAVE_TIME                    | posix | System provides time function
HAVE_TIME_MS                 | embedded | System provides time in milliseconds

<!-- a name "lst:btstackFeatureConfiguration"></a-->
<!-- -->

#define | Explaination
------------------|---------------------------------------------
ENABLE_CLASSIC    | Enable Classic related code in HCI and L2CAP
ENABLE_BLE        | Enable BLE related code in HCI and L2CAP
ENABLE_LOG_DEBUG  | Enable log_debug messages
ENABLE_LOG_ERROR  | Enable log_error messages
ENABLE_LOG_INFO   | Enable log_info messages
ENABLE_LOG_INTO_HCI_DUMP | Log debug messages as part of packet log
ENABLE_SCO_OVER_HCI | Enable SCO over HCI for chipsets that support it (only CC256x ones currently)


## Memory configuration {#sec:memoryConfigurationHowTo}

The structs for services, active connections and remote devices can be
allocated in two different manners:

-   statically from an individual memory pool, whose maximal number of
    elements is defined in the config file. To initialize the static
    pools, you need to call *btstack_memory_init* function. An example
    of memory configuration for a single SPP service with a minimal
    L2CAP MTU is shown in Listing {@lst:memoryConfigurationSPP}.

-   dynamically using the *malloc/free* functions, if HAVE_MALLOC is
    defined in config file.

For each HCI connection, a buffer of size HCI_ACL_PAYLOAD_SIZE is reserved. For fast data transfer, however, a large ACL buffer of 1021 bytes is recommened. The large ACL buffer is required for 3-DH5 packets to be used.

<!-- a name "lst:memoryConfiguration"></a-->
<!-- -->

#define | Explaination 
--------|------------
HCI_ACL_PAYLOAD_SIZE | Max size of HCI ACL payloads
MAX_NR_BNEP_CHANNELS | Max nr. of BNEP channels
MAX_NR_BNEP_SERVICES | Max nr. provides BNEP services
MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES | Max nr. of link key entries cached in RAM
MAX_NR_GATT_CLIENTS | Max nr. of GATT clients
MAX_NR_HCI_CONNECTIONS | Max nr. HCI connections
MAX_NR_HFP_CONNECTIONS | Max nr. HFP connections
MAX_NR_L2CAP_CHANNELS |  Max nr. L2CAP connections
MAX_NR_L2CAP_SERVICES |  Max nr. L2CAP services
MAX_NR_RFCOMM_CHANNELS | Max nr. RFOMMM connections
MAX_NR_RFCOMM_MULTIPLEXERS | Max nr. RFCOMM multiplexers. One multiplexer per HCI connection
MAX_NR_RFCOMM_SERVICES | Max nr. RFCOMM services
MAX_NR_SERVICE_RECORD_ITEMS | Max nr. SDP service records
MAX_NR_SM_LOOKUP_ENTRIES | Max nr. of items in Security Manager lookup queue
MAX_NR_WHITELIST_ENTRIES | Max nr. of items in GAP LE Whitelist to connect to

The memory is set up by calling *btstack_memory_init* function:

    btstack_memory_init();

<!-- a name "lst:memoryConfigurationSPP"></a-->
<!-- -->

    #define HCI_ACL_PAYLOAD_SIZE 52
    #define MAX_NR_HCI_CONNECTIONS 1
    #define MAX_NR_L2CAP_SERVICES  2
    #define MAX_NR_L2CAP_CHANNELS  2
    #define MAX_NR_RFCOMM_MULTIPLEXERS 1
    #define MAX_NR_RFCOMM_SERVICES 1
    #define MAX_NR_RFCOMM_CHANNELS 1
    #define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES  3

Listing: Title. {#lst:memoryConfigurationSPP}

In this example, the size of ACL packets is limited to the minimum of 52 bytes, resulting in an L2CAP MTU of 48 bytes. Only a singleHCI connection can be established at any time. On it, two L2CAP services are provided, which can be active at the same time. Here, these two can be RFCOMM and SDP. Then, memory for one RFCOMM multiplexer is reserved over which one connection can be active. Finally, up to three link keys can be cached in RAM.

<!-- -->

## Source tree structure {#sec:sourceTreeHowTo}

TODO:

- src: with src/classic and src/ble
- chipset: bcm, cc256x, csr, em9301, stlc2500f, tc3566x
- platform: corefoundation, daemon, embedded, posix
- port: ...

## Run loop {#sec:runLoopHowTo}

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
- POSIX: implementation for POSIX systems based on the select() call.
- WICED: implementation for the Broadcom WICED SDK RTOS abstraction that warps FreeRTOS or ThreadX.
- CoreFoundation: implementation for iOS and OS X applications 

Depending on the platform, data sources are either polled (embedded), or the platform provides a way
to wait for a data source to become ready for read or write (POSIX, CoreFoundation), or,
are not used as the HCI transport driver and the run loop are implemented in a different way (WICED).
In any case, the callbacks need to explicitly enabled with the *btstack_run_loop_enable_data_source_callbacks(..)* function.

In your code, you’ll have to configure the run loop before you start it
as shown in Listing [listing:btstackInit]. The application can register
data sources as well as timers, e.g., periodical sampling of sensors, or
communication over the UART.

The run loop is set up by calling *btstack_run_loop_init* function and providing
an instance of the actual run loop. E.g. for the embedded platform, it is:

<!-- -->

    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

The complete Run loop API is provided [here](appendix/apis/#sec:runLoopAPIAppendix).

### Run loop embedded

In the embeded run loop implementation, data sources are constantly polled and 
the system is put to sleep if no IRQ happend during the poll of all data sources.

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
entering sleep mode causing another round of callbacks.

To enable the use of timers, make sure that you defined HAVE_TICK or HAVE_TIME_MS in the
config file.

### Run loop POSIX

The data sources are standard File Descriptors. In the run loop execute implementaion,
select() call is used to wait for file descriptors to become ready to read or write,
while waiting for the next timeout. 

To enable the use of timers, make sure that you defined HAVE_TIME in the config file.

### Run loop CoreFoundation (OS X/iOS)

This run loop directly maps BTstack's data source and timer source with CoreFoundation objects.
It supports ready to read and write similar to the POSIX implementation. The call to
*btstack_run_loop_execute()* then just calls *CFRunLoopRun()*.

To enable the use of timers, make sure that you defined HAVE_TIME in the config file.

### Run loop WICED


WICED SDK API does not provide asynchronous read and write to the UART and no direct way to wait for 
one or more peripherals to become ready. Therefore, BTstack does not provide direct support for data sources.
Instead,  the run loop provides a message queue that  allows to schedule functions calls on its thread via
*btstack_run_loop_wiced_execute_code_on_main_thread()*. 

The HCI transport H4 implementation uses 2 lightweight thread to do the 
blocking read and write operations. When a read or write is complete on
the helper threads, a callback to BTstack is scheduled.


## BTstack initialization {#sec:btstackInitializationHowTo}

To initialize BTstack you need to [initialize the memory](#sec:memoryConfigurationHowTo)
and [the run loop](#sec:runLoopHowTo) respectively, then setup HCI and all needed higher
level protocols.


The HCI initialization has to adapt BTstack to the used platform and
requires four arguments. These are:

-   *Bluetooth hardware control*: The Bluetooth hardware control API can
    provide the HCI layer with a custom initialization script, a
    vendor-specific baud rate change command, and system power
    notifications. It is also used to control the power mode of the
    Bluetooth module, i.e., turning it on/off and putting to sleep. In
    addition, it provides an error handler *hw_error* that is called
    when a Hardware Error is reported by the Bluetooth module. The
    callback allows for persistent logging or signaling of this failure.

    Overall, the struct *btstack_control_t* encapsulates common
    functionality that is not covered by the Bluetooth specification. As
    an example, the *bt_con-trol_cc256x_in-stance* function returns a
    pointer to a control struct suitable for the CC256x chipset.

<!-- -->

    btstack_control_t * control = btstack_chipset_cc256x_instance();

-   *HCI Transport implementation*: On embedded systems, a Bluetooth
    module can be connected via USB or an UART port. BTstack implements
    two UART based protocols: HCI UART Transport Layer (H4) and H4 with
    eHCILL support, a lightweight low-power variant by Texas
    Instruments. These are accessed by linking the appropriate file 
    [src/hci_transport_h4_embedded.c]() resp. [src/hci_transport_h4_ehcill_embedded.c]()
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
    First, the HCI command to change the baud rate is sent, then it is
    necessary to wait for the confirmation event from the Bluetooth
    module. Only now, can the UART baud rate changed. As an example, the
    CC256x has to be initialized at 115200 and can then be used at
    higher speeds.

<!-- -->

    hci_uart_config_t* config = hci_uart_config_cc256x_instance();

-   *Persistent storage* - specifies where to persist data like link
    keys or remote device names. This commonly requires platform
    specific code to access the MCU’s EEPROM of Flash storage. For the
    first steps, BTstack provides a (non) persistent store in memory.
    For more see [here](porting/#sec:persistentStoragePorting).

<!-- -->

    remote_device_db_t * remote_db = &remote_device_db_memory;

After these are ready, HCI is initialized like this:

<!-- -->
    hci_init(transport, config, control, remote_db);

The higher layers only rely on BTstack and are initialized by calling
the respective *\*_init* function. These init functions register
themselves with the underlying layer. In addition, the application can
register packet handlers to get events and data as explained in the
following section.


## Services {#sec:servicesHowTo}

One important construct of BTstack is *service*. A service represents a
server side component that handles incoming connections. So far, BTstack
provides L2CAP and RFCOMM services. An L2CAP service handles incoming
connections for an L2CAP channel and is registered with its protocol
service multiplexer ID (PSM). Similarly, an RFCOMM service handles
incoming RFCOMM connections and is registered with the RFCOMM channel
ID. Outgoing connections require no special registration, they are
created by the application when needed.



## Where to get data - packet handlers {#sec:packetHandlersHowTo}


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

-   HCI packet handler - handles HCI and general BTstack events if L2CAP
    is not used (rare case).

-   L2CAP packet handler - handles HCI and general BTstack events.

-   L2CAP service packet handler - handles incoming L2CAP connections,
    i.e., channels initiated by the remote.

-   L2CAP channel packet handler - handles outgoing L2CAP connections,
    i.e., channels initiated internally.

-   RFCOMM packet handler - handles RFCOMM incoming/outgoing events and
    data.

These handlers are registered with the functions listed in Table
{@tbl:registeringFunction}.

  ------------------------------ --------------------------------------
                  Packet Handler Registering Function
              HCI packet handler *hci_register_packet_handler*
            L2CAP packet handler *l2cap_register_packet_handler*
    L2CAP service packet handler *l2cap_register_service*
    L2CAP channel packet handler *l2cap_create_channel*
           RFCOMM packet handler *rfcomm_register_packet_handler*
  ------------------------------ --------------------------------------

Table: Functions for registering packet handlers. {#tbl:registeringFunction}

HCI and general BTstack events are delivered to the packet handler
specified by *l2cap_register_packet_handler* function, or
*hci_register_packet_handler*, if L2CAP is not used. In L2CAP,
BTstack discriminates incoming and outgoing connections, i.e., event and
data packets are delivered to different packet handlers. Outgoing
connections are used access remote services, incoming connections are
used to provide services. For incoming connections, the packet handler
specified by *l2cap_register_service* is used. For outgoing
connections, the handler provided by *l2cap_create_channel*
is used. Currently, RFCOMM provides only a single packet handler
specified by *rfcomm_register_packet_handler* for all RFCOMM
connections, but this will be fixed in the next API overhaul.

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



## Bluetooth HCI Packet Logs {#sec:packetlogsHowTo}


If things don't work as expected, having a look at the data exchanged
between BTstack and the Bluetooth chipset often helps.

For this, BTstack provides a configurable packet logging mechanism via hci_dump.h:

    // formats: HCI_DUMP_BLUEZ, HCI_DUMP_PACKETLOGGER, HCI_DUMP_STDOUT
    void hci_dump_open(const char *filename, hci_dump_format_t format);

On POSIX systems, you can call *hci_dump_open* with a path and *HCI_DUMP_BLUEZ* 
or *HCI_DUMP_PACKETLOGGER* in the setup, i.e., before entering the run loop.
The resulting file can be analyzed with Wireshark 
or the Apple's PacketLogger tool.

On embedded systems without a file system, you still can call *hci_dump_open(NULL, HCI_DUMP_STDOUT)*.
It will log all HCI packets to the consolve via printf.
If you capture the console output, incl. your own debug messages, you can use 
the create_packet_log.py tool in the tools folder to convert a text output into a
PacketLogger file.

In addition to the HCI packets, you can also enable BTstack's debug information by adding

    #define ENABLE_LOG_INFO 
    #define ENABLE_LOG_ERROR

to the btstack_config.h and recompiling your application.

