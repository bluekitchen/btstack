
BTstack implements a set of basic Bluetooth protocols. To make use of
these to connect to other devices or to provide own services, BTstack
has to be properly configured during application startup.

In the following, we provide an overview of the memory management, the
run loop, and services that are necessary to setup BTstack. From the
point when the run loop is executed, the application runs as a finite
state machine, which processes events received from BTstack. BTstack
groups events logically and provides them over packet handlers, of which
an overview is provided here. Finally, we describe the RFCOMM
credit-based flow-control, which may be necessary for
resource-constraint devices.

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

<!-- a name "lst:memoryConfigurationSPP"></a-->
<!-- -->

    #define HCI_ACL_PAYLOAD_SIZE 52
    #define MAX_SPP_CONNECTIONS 1
    #define MAX_NO_HCI_CONNECTIONS MAX_SPP_CONNECTIONS
    #define MAX_NO_L2CAP_SERVICES  2
    #define MAX_NO_L2CAP_CHANNELS  (1+MAX_SPP_CONNECTIONS)
    #define MAX_NO_RFCOMM_MULTIPLEXERS MAX_SPP_CONNECTIONS
    #define MAX_NO_RFCOMM_SERVICES 1
    #define MAX_NO_RFCOMM_CHANNELS MAX_SPP_CONNECTIONS
    #define MAX_NO_DB_MEM_DEVICE_NAMES  0
    #define MAX_NO_DB_MEM_LINK_KEYS  3
    #define MAX_NO_DB_MEM_SERVICES 1

Listing: Title. {#lst:memoryConfigurationSPP}

If both HAVE_MALLOC and maximal size of a pool are defined in the
config file, the statical allocation will take precedence. In case that
both are omitted, an error will be raised.

The memory is set up by calling *btstack_memory_init* function:

<!-- -->

    btstack_memory_init();


## Run loop {#sec:runLoopHowTo}

BTstack uses a run loop to handle incoming data and to schedule work.
The run loop handles events from two different types of sources: data
sources and timers. Data sources represent communication interfaces like
an UART or an USB driver. Timers are used by BTstack to implement
various Bluetooth-related timeouts. They can also be used to handle
periodic events.

Data sources and timers are represented by the *data_source_t* and
*timer_source_t* structs respectively. Each of these structs contain a
linked list node and a pointer to a callback function. All active timers
and data sources are kept in link lists. While the list of data sources
is unsorted, the timers are sorted by expiration timeout for efficient
processing.

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
*embedded_trigger* function. The call to *embedded_trigger* sets an
internal flag that is checked in the critical section just before
entering sleep mode.

Timers are single shot: a timer will be removed from the timer list
before its event handler callback is executed. If you need a periodic
timer, you can re-register the same timer source in the callback
function, as shown in Listing [PeriodicTimerHandler]. Note that BTstack
expects to get called periodically to keep its time, see Section
[on time abstraction](#sec:timeAbstractionPorting) for more on the 
tick hardware abstraction.

The run loop is set up by calling *run_loop_init* function for
embedded systems:

<!-- -->

    run_loop_init(RUN_LOOP_EMBEDDED);

The Run loop API is provided [here](appendix/apis/#sec:runLoopAPIAppendix). To
enable the use of timers, make sure that you defined HAVE_TICK in the
config file.

In your code, you’ll have to configure the run loop before you start it
as shown in Listing [listing:btstackInit]. The application can register
data sources as well as timers, e.g., periodical sampling of sensors, or
communication over the UART.


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

    Overall, the struct *bt_control_t* encapsulates common
    functionality that is not covered by the Bluetooth specification. As
    an example, the *bt_con-trol_cc256x_in-stance* function returns a
    pointer to a control struct suitable for the CC256x chipset.

<!-- -->

    bt_control_t * control = bt_control_cc256x_instance();

-   *HCI Transport implementation*: On embedded systems, a Bluetooth
    module can be connected via USB or an UART port. BTstack implements
    two UART based protocols: HCI UART Transport Layer (H4) and H4 with
    eHCILL support, a lightweight low-power variant by Texas
    Instruments. These are accessed by linking the appropriate file 
    [src/hci_transport_h4_dma.c]() resp. [src/hci_transport_h4_ehcill_dma.c]()
    and then getting a pointer to HCI Transport implementation.
    For more information on adapting HCI Transport to different
    environments, see [here](porting/#sec:hciTransportPorting).

<!-- -->

    hci_transport_t * transport = hci_transport_h4_dma_instance();

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
    L2CAP service packet handler *l2cap_register_service_internal*
    L2CAP channel packet handler *l2cap_create_channel_internal*
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
connections, the handler provided by *l2cap_create_channel_internal*
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
data registered by *l2cap_create_channel_internal*; after that
keyboard can reconnect by itself. For this, you need to register L2CAP
services for the HID Control and HID Interrupt PSMs using
*l2cap_register_service_internal*. In this call, you’ll also specify
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

to the btstack-config.h and recompiling your application.

