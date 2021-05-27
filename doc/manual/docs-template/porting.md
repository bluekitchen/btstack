#

In this section, we highlight the BTstack components that need to be
adjusted for different hardware platforms.


## Time Abstraction Layer {#sec:timeAbstractionPorting}

BTstack requires a way to learn about passing time.
*btstack_run_loop_embedded.c* supports two different modes: system ticks or a
system clock with millisecond resolution. BTstack’s timing requirements
are quite low as only Bluetooth timeouts in the second range need to be
handled.


### Tick Hardware Abstraction {#sec:tickAbstractionPorting}


If your platform doesn’t require a system clock or if you already have a
system tick (as it is the default with CMSIS on ARM Cortex devices), you
can use that to implement BTstack’s time abstraction in
*include/btstack/hal_tick.h\>*.

For this, you need to define *HAVE_EMBEDDED_TICK* in *btstack_config.h*:

    #define HAVE_EMBEDDED_TICK

Then, you need to implement the functions *hal_tick_init* and
*hal_tick_set_handler*, which will be called during the
initialization of the run loop.

    void hal_tick_init(void);
    void hal_tick_set_handler(void (*tick_handler)(void));
    int  hal_tick_get_tick_period_in_ms(void);

After BTstack calls *hal_tick_init()* and
*hal_tick_set_handler(tick_handler)*, it expects that the
*tick_handler* gets called every
*hal_tick_get_tick_period_in_ms()* ms.


### Time MS Hardware Abstraction {#sec:timeMSAbstractionPorting}


If your platform already has a system clock or it is more convenient to
provide such a clock, you can use the Time MS Hardware Abstraction in
*include/btstack/hal_time_ms.h*.

For this, you need to define *HAVE_EMBEDDED_TIME_MS* in *btstack_config.h*:

    #define HAVE_EMBEDDED_TIME_MS

Then, you need to implement the function *hal_time_ms()*, which will
be called from BTstack’s run loop and when setting a timer for the
future. It has to return the time in milliseconds.

    uint32_t hal_time_ms(void);


## Bluetooth Hardware Control API {#sec:btHWControlPorting}


The Bluetooth hardware control API can provide the HCI layer with a
custom initialization script, a vendor-specific baud rate change
command, and system power notifications. It is also used to control the
power mode of the Bluetooth module, i.e., turning it on/off and putting
to sleep. In addition, it provides an error handler *hw_error* that is
called when a Hardware Error is reported by the Bluetooth module. The
callback allows for persistent logging or signaling of this failure.

Overall, the struct *btstack_control_t* encapsulates common functionality
that is not covered by the Bluetooth specification. As an example, the
*btstack_chipset_cc256x_in-stance* function returns a pointer to a control
struct suitable for the CC256x chipset.



## HCI Transport Implementation {#sec:hciTransportPorting}


On embedded systems, a Bluetooth module can be connected via USB or an
UART port. BTstack implements three UART based protocols for carrying HCI
commands, events and data between a host and a Bluetooth module: HCI
UART Transport Layer (H4), H4 with eHCILL support, a lightweight
low-power variant by Texas Instruments, and the Three-Wire UART Transport Layer (H5).


### HCI UART Transport Layer (H4) {#sec:hciUARTPorting}


Most embedded UART interfaces operate on the byte level and generate a
processor interrupt when a byte was received. In the interrupt handler,
common UART drivers then place the received data in a ring buffer and
set a flag for further processing or notify the higher-level code, i.e.,
in our case the Bluetooth stack.

Bluetooth communication is packet-based and a single packet may contain
up to 1021 bytes. Calling a data received handler of the Bluetooth stack
for every byte creates an unnecessary overhead. To avoid that, a
Bluetooth packet can be read as multiple blocks where the amount of
bytes to read is known in advance. Even better would be the use of
on-chip DMA modules for these block reads, if available.

The BTstack UART Hardware Abstraction Layer API reflects this design
approach and the underlying UART driver has to implement the following
API:

    void hal_uart_dma_init(void);
    void hal_uart_dma_set_block_received(void (*block_handler)(void));
    void hal_uart_dma_set_block_sent(void (*block_handler)(void));
    int  hal_uart_dma_set_baud(uint32_t baud);
    void hal_uart_dma_send_block(const uint8_t *buffer, uint16_t len);
    void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len);
     

The main HCI H4 implementations for embedded system is
*hci_h4_transport-_dma* function. This function calls the following
sequence: *hal_uart_dma_init*, *hal_uart_dma_set_block_received*
and *hal_uart_dma_set_block_sent* functions. this sequence, the HCI
layer will start packet processing by calling
*hal_uart-_dma_receive_block* function. The HAL implementation is
responsible for reading the requested amount of bytes, stopping incoming
data via the RTS line when the requested amount of data was received and
has to call the handler. By this, the HAL implementation can stay
generic, while requiring only three callbacks per HCI packet.

### H4 with eHCILL support

With the standard H4 protocol interface, it is not possible for either
the host nor the baseband controller to enter a sleep mode. Besides the
official H5 protocol, various chip vendors came up with proprietary
solutions to this. The eHCILL support by Texas Instruments allows both
the host and the baseband controller to independently enter sleep mode
without loosing their synchronization with the HCI H4 Transport Layer.
In addition to the IRQ-driven block-wise RX and TX, eHCILL requires a
callback for CTS interrupts.

    void hal_uart_dma_set_cts_irq_handler(void(*cts_irq_handler)(void));
    void hal_uart_dma_set_sleep(uint8_t sleep);


### H5

H5, makes use of the SLIP protocol to transmit a packet and can deal
with packet loss and bit-errors by retransmission. Since it can recover
from packet loss, it's also possible for either side to enter sleep
mode without loosing synchronization.

The use of hardware flow control in H5 is optional, however, since
BTstack uses hardware flow control to avoid packet buffers, it's
recommended to only use H5 with RTS/CTS as well.

For porting, the implementation follows the regular H4 protocol described above.

## Persistent Storage APIs {#sec:persistentStoragePorting}

On embedded systems there is no generic way to persist data like link
keys or remote device names, as every type of a device has its own
capabilities, particularities and limitations. The persistent storage
APIs provides an interface to implement concrete drivers for a particular
system. 

### Link Key DB

As an example and for testing purposes, BTstack provides the
memory-only implementation *btstack_link_key_db_memory*. An
implementation has to conform to the interface in Listing [below](#lst:persistentDB).

~~~~ {#lst:persistentDB .c caption="{Persistent storage interface.}"}
    
    typedef struct {
        // management
        void (*open)();
        void (*close)();
        
        // link key
        int  (*get_link_key)(bd_addr_t bd_addr, link_key_t link_key);
        void (*put_link_key)(bd_addr_t bd_addr, link_key_t key);
        void (*delete_link_key)(bd_addr_t bd_addr);
    } btstack_link_key_db_t;
~~~~ 
