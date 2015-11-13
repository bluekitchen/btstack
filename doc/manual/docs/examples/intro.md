
In this section, we will describe a number of examples from the
*example/embedded* folder. To allow code-reuse with different platforms
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
      run_loop_init(RUN_LOOP_EMBEDDED);
          
      // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
      hci_dump_open(NULL, HCI_DUMP_STDOUT);

      // init HCI
      hci_transport_t    * transport = hci_transport_h4_dma_instance();
      remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
      hci_init(transport, NULL, NULL, remote_db);

      // setup example    
      btstack_main(argc, argv);

      // go
      run_loop_execute();    
    }
    
~~~~ 

First, BTstack’s memory pools are setup up. Then, the standard run loop
implementation for embedded systems is selected.

The call to *hci_dump_open* configures BTstack to output all Bluetooth
packets and it’s own debug and error message via printf. The Python
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