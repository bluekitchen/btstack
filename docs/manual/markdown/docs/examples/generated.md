


- Hello World example:

    - [led_counter](#section:ledcounter): Hello World: blinking LED without Bluetooth.

- GAP example:

    - [gap_inquiry](#section:gapinquiry): GAP Inquiry Example.

- SDP Queries examples:

    - [sdp_general_query](#section:sdpgeneralquery): Dump remote SDP Records.

    - [sdp_bnep_query](#section:sdpbnepquery): Dump remote BNEP PAN protocol UUID and L2CAP PSM.

- SPP Server examples:

    - [spp_counter](#section:sppcounter): SPP Server - Heartbeat Counter over RFCOMM.

    - [spp_flowcontrol](#section:sppflowcontrol): SPP Server - Flow Control.

- BNEP/PAN example:

    - [panu_demo](#section:panudemo): PANU Demo.

- Low Energy examples:

    - [gatt_browser](#section:gattbrowser): GATT Client - Discovering primary services and their characteristics.

    - [le_counter](#section:lecounter): LE Peripheral - Heartbeat Counter over GATT.

- Dual Mode example:

    - [spp_and_le_counter](#section:sppandlecounter): Dual mode example.




## led_counter: Hello World: blinking LED without Bluetooth
<a name="section:ledcounter"></a>



  The example demonstrates how to provide a periodic timer to toggle an LED and send debug messages to the console as a minimal BTstack test.

### Periodic Timer Setup 


  As timers in BTstack are single shot, the periodic counter is implemented by re-registering the timer source in the heartbeat handler callback function. [code snippet below](#ledcounter:LEDToggler) shows heartbeat handler adapted to periodically toggle an LED and print number of toggles.  


<a name="ledcounter:LEDToggler"></a>
<!-- -->

    static void heartbeat_handler(timer_source_t *ts){   
      // increment counter
      char lineBuffer[30];
      sprintf(lineBuffer, "BTstack counter %04u\n\r", ++counter);
      puts(lineBuffer);
      
      // toggle LED
      hal_led_toggle();
    
      // re-register timer
      run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
      run_loop_add_timer(&heartbeat);
    } 




### Main Application Setup


  [code snippet below](#ledcounter:MainConfiguration) shows main application code. It configures the heartbeat tier and adds it to the run loop.


<a name="ledcounter:MainConfiguration"></a>
<!-- -->

    int btstack_main(int argc, const char * argv[]);
    int btstack_main(int argc, const char * argv[]){
    
      // set one-shot timer
      heartbeat.process = &heartbeat_handler;
      run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
      run_loop_add_timer(&heartbeat);
    
      printf("Running...\n\r");
      return 0;
    }




## gap_inquiry: GAP Inquiry Example
<a name="section:gapinquiry"></a>



  The Generic Access Profile (GAP) defines how Bluetooth devices discover and establish a connection with each other. In this example, the application discovers  surrounding Bluetooth devices and collects their Class of Device (CoD), page scan mode, clock offset, and RSSI. After that, the remote name of each device is requested. In the following section we outline the Bluetooth logic part, i.e., how the packet handler handles the asynchronous events and data packets.

### Bluetooth Logic 


  The Bluetooth logic is implemented as a state machine within the packet handler. In this example, the following states are passed sequentially: INIT, and ACTIVE.

  In INIT, an inquiry  scan is started, and the application transits to  ACTIVE state.

  In ACTIVE, the following events are processed:
 
- Inquiry result event: the list of discovered devices is processed and the  Class of Device (CoD), page scan mode, clock offset, and RSSI are stored in a table.
- Inquiry complete event: the remote name is requested for devices without a fetched  name. The state of a remote name can be one of the following:  REMOTE_NAME_REQUEST, REMOTE_NAME_INQUIRED, or REMOTE_NAME_FETCHED.
- Remote name cached event: prints cached remote names provided by BTstack,  if persistent storage is provided.
- Remote name request complete event: the remote name is stored in the table and the  state is updated to REMOTE_NAME_FETCHED. The query of remote names is continued.



  For more details on discovering remote devices, please see [here](#section:DiscoverRemoteDevices).

### Main Application Setup


  [code snippet below](#gapinquiry:MainConfiguration) shows main application code. It registers the HCI packet handler and starts the Bluetooth stack.


<a name="gapinquiry:MainConfiguration"></a>
<!-- -->

    int btstack_main(int argc, const char * argv[]);
    int btstack_main(int argc, const char * argv[]) {
      hci_register_packet_handler(packet_handler);
    
      // turn on!
    	hci_power_control(HCI_POWER_ON);
    	  
      return 0;
    }




## sdp_general_query: Dump remote SDP Records
<a name="section:sdpgeneralquery"></a>



  The example shows how the SDP Client is used to get a list of  service records on a remote device. 

### SDP Client Setup


  SDP is based on L2CAP. To receive SDP query events you must register a callback, i.e. query handler, with the SPD parser, as shown in  [code snippet below](#sdpgeneralquery:SDPClientInit). Via this handler, the SDP client will receive the following events:
 
- SDP_QUERY_ATTRIBUTE_VALUE containing the results of the query in chunks,
- SDP_QUERY_COMPLETE indicating the end of the query and the status




<a name="sdpgeneralquery:SDPClientInit"></a>
<!-- -->

    static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static void handle_sdp_client_query_result(sdp_query_event_t * event);
    
    static void sdp_client_init(){
      // init L2CAP
      l2cap_init();
      l2cap_register_packet_handler(packet_handler);
    
      sdp_parser_init();
      sdp_parser_register_callback(handle_sdp_client_query_result);
    }




### SDP Client Query 


  To trigger an SDP query to get the a list of service records on a remote device, you need to call sdp_general_query_for_uuid() with the remote address and the UUID of the public browse group, as shown in [code snippet below](#sdpgeneralquery:SDPQueryUUID).  In this example we used fixed address of the remote device shown in [code snippet below](#sdpgeneralquery:Remote). Please update it with the address of a device in your vicinity, e.g., one reported by the GAP Inquiry example in the previous section.


<a name="sdpgeneralquery:Remote"></a>
<!-- -->

    static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};





<a name="sdpgeneralquery:SDPQueryUUID"></a>
<!-- -->

    static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
      // printf("packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);
    
      if (packet_type != HCI_EVENT_PACKET) return;
      uint8_t event = packet[0];
    
      switch (event) {
        case BTSTACK_EVENT_STATE:
          if (packet[2] == HCI_STATE_WORKING){
            sdp_general_query_for_uuid(remote, SDP_PublicBrowseGroup);
          }
          break;
        default:
          break;
      }
    }




### Handling SDP Client Query Results 


  The SDP Client returns the results of the query in chunks. Each result packet contains the record ID, the Attribute ID, and a chunk of the Attribute value. In this example, we append new chunks for the same Attribute ID in a large buffer, see [code snippet below](#sdpgeneralquery:HandleSDPQUeryResult).

 To save memory, it's also possible to process these chunks directly by a custom stream parser, similar to the way XML files are parsed by a SAX parser. Have a look at \emph{src/sdp_query_rfcomm.c} which retrieves the RFCOMM channel number and the service name.


<a name="sdpgeneralquery:HandleSDPQUeryResult"></a>
<!-- -->

    static void handle_sdp_client_query_result(sdp_query_event_t * event){
      sdp_query_attribute_value_event_t * ve;
      sdp_query_complete_event_t * ce;
    
      switch (event->type){
        case SDP_QUERY_ATTRIBUTE_VALUE:
          ve = (sdp_query_attribute_value_event_t*) event;
          
          // handle new record
          if (ve->record_id != record_id){
            record_id = ve->record_id;
            printf("\n---\nRecord nr. %u\n", record_id);
          }
    
          assertBuffer(ve->attribute_length);
    
          attribute_value[ve->data_offset] = ve->data;
          if ((uint16_t)(ve->data_offset+1) == ve->attribute_length){
             printf("Attribute 0x%04x: ", ve->attribute_id);
             de_dump_data_element(attribute_value);
          }
          break;
        case SDP_QUERY_COMPLETE:
          ce = (sdp_query_complete_event_t*) event;
          printf("General query done with status %d.\n\n", ce->status);
          exit(0);
          break;
      }
    }




## sdp_bnep_query: Dump remote BNEP PAN protocol UUID and L2CAP PSM
<a name="section:sdpbnepquery"></a>



  The example shows how the SDP Client is used to get all BNEP service records from a remote device. It extracts the remote BNEP PAN protocol  UUID and the L2CAP PSM, which are needed to connect to a remote BNEP service.

### SDP Client Setup


  As with the previous example, you must register a callback, i.e. query handler, with the SPD parser, as shown in  [code snippet below](#sdpbnepquery:SDPClientInit). Via this handler, the SDP client will receive events:
 
- SDP_QUERY_ATTRIBUTE_VALUE containing the results of the query in chunks,
- SDP_QUERY_COMPLETE reporting the status and the end of the query. 




<a name="sdpbnepquery:SDPClientInit"></a>
<!-- -->

    static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static void handle_sdp_client_query_result(sdp_query_event_t * event);
    
    static void sdp_client_init(){
      // init L2CAP
      l2cap_init();
      l2cap_register_packet_handler(packet_handler);
    
      sdp_parser_init();
      sdp_parser_register_callback(handle_sdp_client_query_result);
    }




### SDP Client Query 



<a name="sdpbnepquery:SDPQueryUUID"></a>
<!-- -->

    static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
      if (packet_type != HCI_EVENT_PACKET) return;
      uint8_t event = packet[0];
    
      switch (event) {
        case BTSTACK_EVENT_STATE:
          // BTstack activated, get started 
          if (packet[2] == HCI_STATE_WORKING){
            printf("Start SDP BNEP query.\n");
            sdp_general_query_for_uuid(remote, SDP_BNEPProtocol);
          }
          break;
        default:
          break;
      }
    }




### Handling SDP Client Query Result 


  The SDP Client returns the result of the query in chunks. Each result packet contains the record ID, the Attribute ID, and a chunk of the Attribute value, see [code snippet below](#sdpbnepquery:HandleSDPQUeryResult). Here, we show how to parse the Service Class ID List and Protocol Descriptor List, as they contain  the BNEP Protocol UUID and L2CAP PSM respectively.


<a name="sdpbnepquery:HandleSDPQUeryResult"></a>
<!-- -->

    static void handle_sdp_client_query_result(sdp_query_event_t * event){
    ...
    
            switch(ve->attribute_id){
              // 0x0001 "Service Class ID List"
              case SDP_ServiceClassIDList:
                if (de_get_element_type(attribute_value) != DE_DES) break;
                for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)){
                  uint8_t * element = des_iterator_get_element(&des_list_it);
                  if (de_get_element_type(element) != DE_UUID) continue;
                  uint32_t uuid = de_get_uuid32(element);
                  switch (uuid){
                    case PANU_UUID:
                    case NAP_UUID:
                    case GN_UUID:
                      printf(" ** Attribute 0x%04x: BNEP PAN protocol UUID: %04x\n", ve->attribute_id, uuid);
                      break;
                    default:
                      break;
                  }
                }
                break;
    ...
              case SDP_ProtocolDescriptorList:{
                  printf(" ** Attribute 0x%04x: ", ve->attribute_id);
                  
                  uint16_t l2cap_psm = 0;
                  uint16_t bnep_version = 0;
                  for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)){
                    if (des_iterator_get_type(&des_list_it) != DE_DES) continue;
                    uint8_t * des_element = des_iterator_get_element(&des_list_it);
                    des_iterator_init(&prot_it, des_element);
                    uint8_t * element = des_iterator_get_element(&prot_it);
                    
                    if (de_get_element_type(element) != DE_UUID) continue;
                    uint32_t uuid = de_get_uuid32(element);
                    switch (uuid){
                      case SDP_L2CAPProtocol:
                        if (!des_iterator_has_more(&prot_it)) continue;
                        des_iterator_next(&prot_it);
                        de_element_get_uint16(des_iterator_get_element(&prot_it), &l2cap_psm);
                        break;
                      case SDP_BNEPProtocol:
                        if (!des_iterator_has_more(&prot_it)) continue;
                        des_iterator_next(&prot_it);
                        de_element_get_uint16(des_iterator_get_element(&prot_it), &bnep_version);
                        break;
                      default:
                        break;
                    }
                  }
                  printf("l2cap_psm 0x%04x, bnep_version 0x%04x\n", l2cap_psm, bnep_version);
                }
                break;
    ...
    }




  The Service Class ID List is a Data Element Sequence (DES) of UUIDs.  The BNEP PAN protocol UUID is within this list.

  The Protocol Descriptor List is DES  which contains one DES for each protocol. For PAN serivces, it contains a DES with the L2CAP Protocol UUID and a PSM, and another DES with the BNEP UUID and the the BNEP version.

## spp_counter: SPP Server - Heartbeat Counter over RFCOMM
<a name="section:sppcounter"></a>



  The Serial port profile (SPP) is widely used as it provides a serial port over Bluetooth. The SPP counter example demonstrates how to setup an SPP service, and provide a periodic timer over RFCOMM.   

### SPP Service Setup 


  To provide an SPP service, the L2CAP, RFCOMM, and SDP protocol layers  are required. After setting up an RFCOMM service with channel nubmer RFCOMM_SERVER_CHANNEL, an SDP record is created and registered with the SDP server. Example code for SPP service setup is provided in [code snippet below](#sppcounter:SPPSetup). The SDP record created by function sdp_create_spp_service consists of a basic SPP definition that uses the provided RFCOMM channel ID and service name. For more details, please have a look at it in \path{src/sdp_util.c}.  The SDP record is created on the fly in RAM and is deterministic. To preserve valuable RAM, the result could be stored as constant data inside the ROM.   


<a name="sppcounter:SPPSetup"></a>
<!-- -->

    void spp_service_setup(){
      l2cap_init();
      l2cap_register_packet_handler(packet_handler);
    
      rfcomm_init();
      rfcomm_register_packet_handler(packet_handler);
      rfcomm_register_service_internal(NULL, RFCOMM_SERVER_CHANNEL, 0xffff);  // reserved channel, mtu limited by l2cap
    
      // init SDP, create record for SPP and register with SDP
      sdp_init();
      memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    ...
      service_record_item_t * service_record_item = (service_record_item_t *) spp_service_buffer;
      sdp_create_spp_service( (uint8_t*) &service_record_item->service_record, RFCOMM_SERVER_CHANNEL, "SPP Counter");
      printf("SDP service buffer size: %u\n", (uint16_t) (sizeof(service_record_item_t) + de_get_len((uint8_t*) &service_record_item->service_record)));
      sdp_register_service_internal(NULL, service_record_item);
    ...
    }




### Periodic Timer Setup


  The heartbeat handler increases the real counter every second,  and sends a text string with the counter value, as shown in [code snippet below](#sppcounter:PeriodicCounter). 


<a name="sppcounter:PeriodicCounter"></a>
<!-- -->

    static timer_source_t heartbeat;
    
    static void  heartbeat_handler(struct timer *ts){
      static int counter = 0;
    
      if (rfcomm_channel_id){
        char lineBuffer[30];
        sprintf(lineBuffer, "BTstack counter %04u\n", ++counter);
        printf("%s", lineBuffer);
        if (rfcomm_can_send_packet_now(rfcomm_channel_id)) {
          int err = rfcomm_send_internal(rfcomm_channel_id, (uint8_t*) lineBuffer, strlen(lineBuffer));  
          if (err) {
            log_error("rfcomm_send_internal -> error 0X%02x", err);  
          }
        }   
      }
      run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
      run_loop_add_timer(ts);
    } 
    
    static void one_shot_timer_setup(){
      // set one-shot timer
      heartbeat.process = &heartbeat_handler;
      run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
      run_loop_add_timer(&heartbeat);
    }




### Bluetooth Logic 


  The Bluetooth logic is implemented within the  packet handler, see [code snippet below](#sppcounter:SppServerPacketHandler). In this example,  the following events are passed sequentially: 
 
- BTSTACK_EVENT_STATE,
- HCI_EVENT_PIN_CODE_REQUEST (Standard pairing) or \\ HCI_EVENT_USER_CONFIRMATION_REQUEST \\ (Secure Simple Pairing),
- RFCOMM_EVENT_INCOMING_CONNECTION,
- RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE, and
- RFCOMM_EVENT_CHANNEL_CLOSED



  Upon receiving HCI_EVENT_PIN_CODE_REQUEST event, we need to handle authentication. Here, we use a fixed PIN code "0000".

 When HCI_EVENT_USER_CONFIRMATION_REQUEST is received, the user will be  asked to accept the pairing request. If the IO capability is set to  SSP_IO_CAPABILITY_DISPLAY_YES_NO, the request will be automatically accepted.

 The RFCOMM_EVENT_INCOMING_CONNECTION event indicates an incoming connection. Here, the connection is accepted. More logic is need, if you want to handle connections from multiple clients. The incoming RFCOMM connection event contains the RFCOMM channel number used during the SPP setup phase and the newly assigned RFCOMM channel ID that is used by all BTstack commands and events.

 If RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE event returns status greater then 0, then the channel establishment has failed (rare case, e.g., client crashes). On successful connection, the RFCOMM channel ID and MTU for this channel are made available to the heartbeat counter. After opening the RFCOMM channel,  the communication between client and the application takes place. In this example, the timer handler increases the real counter every second. 


<a name="sppcounter:SppServerPacketHandler"></a>
<!-- -->

    static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    ...
            case HCI_EVENT_PIN_CODE_REQUEST:
              // pre-ssp: inform about pin code request
              printf("Pin code request - using '0000'\n");
              bt_flip_addr(event_addr, &packet[2]);
              hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
              break;
    
            case HCI_EVENT_USER_CONFIRMATION_REQUEST:
              // ssp: inform about user confirmation request
              printf("SSP User Confirmation Request with numeric value '%06u'\n", READ_BT_32(packet, 8));
              printf("SSP User Confirmation Auto accept\n");
              break;
    
            case RFCOMM_EVENT_INCOMING_CONNECTION:
              // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
              bt_flip_addr(event_addr, &packet[2]); 
              rfcomm_channel_nr = packet[8];
              rfcomm_channel_id = READ_BT_16(packet, 9);
              printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
              rfcomm_accept_connection_internal(rfcomm_channel_id);
              break;
             
            case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
              // data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
              if (packet[2]) {
                printf("RFCOMM channel open failed, status %u\n", packet[2]);
              } else {
                rfcomm_channel_id = READ_BT_16(packet, 12);
                mtu = READ_BT_16(packet, 14);
                printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_channel_id, mtu);
              }
              break;
    ...
    }




## spp_flowcontrol: SPP Server - Flow Control
<a name="section:sppflowcontrol"></a>



  This example adds explicit flow control for incoming RFCOMM data to the  SPP heartbeat counter example. We will highlight the changes compared to the  SPP counter example. 

### SPP Service Setup   


  [code snippet below](#sppflowcontrol:explicitFlowControl) shows how to provide one initial credit during RFCOMM service initialization. Please note that providing a single credit effectively reduces the credit-based (sliding window) flow control to a stop-and-wait flow control that limits the data throughput substantially.  


<a name="sppflowcontrol:explicitFlowControl"></a>
<!-- -->

    static void spp_service_setup(){   
      // init L2CAP
      l2cap_init();
      l2cap_register_packet_handler(packet_handler);
      
      // init RFCOMM
      rfcomm_init();
      rfcomm_register_packet_handler(packet_handler);
      // reserved channel, mtu limited by l2cap, 1 credit
      rfcomm_register_service_with_initial_credits_internal(NULL, rfcomm_channel_nr, 0xffff, 1);  
    
      // init SDP, create record for SPP and register with SDP
      sdp_init();
      memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
      service_record_item_t * service_record_item = (service_record_item_t *) spp_service_buffer;
      sdp_create_spp_service( (uint8_t*) &service_record_item->service_record, 1, "SPP Counter");
      printf("SDP service buffer size: %u\n\r", (uint16_t) (sizeof(service_record_item_t) + de_get_len((uint8_t*) &service_record_item->service_record)));
      sdp_register_service_internal(NULL, service_record_item);
    }




### Periodic Timer Setup  


  Explicit credit management is recommended when received RFCOMM data cannot be processed immediately. In this example, delayed processing of received data is simulated with the help of a periodic timer as follows. When the packet handler receives a data packet, it does not provide a new credit, it sets a flag instead, see [code snippet below](#sppflowcontrol:phManual).  If the flag is set, a new credit will be granted by the heartbeat handler, introducing a delay of up to 1 second. The heartbeat handler code is shown in [code snippet below](#sppflowcontrol:hbhManual). 


<a name="sppflowcontrol:hbhManual"></a>
<!-- -->

    static void  heartbeat_handler(struct timer *ts){
      if (rfcomm_send_credit){
        rfcomm_grant_credits(rfcomm_channel_id, 1);
        rfcomm_send_credit = 0;
      }
      run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
      run_loop_add_timer(ts);
    } 





<a name="sppflowcontrol:phManual"></a>
<!-- -->

    // Bluetooth logic
    static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    ...
        case RFCOMM_DATA_PACKET:
          for (i=0;i<size;i++){
            putchar(packet[i]);
          };
          putchar('\n');
          rfcomm_send_credit = 1;
          break;
    ...
    }




## panu_demo: PANU Demo
<a name="section:panudemo"></a>



  This example implements both a PANU client and a server. In server mode, it  sets up a BNEP server and registers a PANU SDP record and waits for incoming connections. In client mode, it connects to a remote device, does an SDP Query to identify the PANU service and initiates a BNEP connection.

### Main application configuration


  In the application configuration, L2CAP and BNEP are initialized and a BNEP service, for server mode, is registered, before the Bluetooth stack gets started, as shown in [code snippet below](#panudemo:PanuSetup).


<a name="panudemo:PanuSetup"></a>
<!-- -->

    static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static void handle_sdp_client_query_result(sdp_query_event_t *event);
    
    static void panu_setup(){
      // Initialize L2CAP 
      l2cap_init();
      l2cap_register_packet_handler(packet_handler);
    
      // Initialise BNEP
      bnep_init();
      bnep_register_packet_handler(packet_handler);
      // Minimum L2CAP MTU for bnep is 1691 bytes
      bnep_register_service(NULL, SDP_PANU, 1691);  
    
      // Initialise SDP 
      sdp_parser_init();
      sdp_parser_register_callback(handle_sdp_client_query_result);
    }




### TUN / TAP interface routines 


  This example requires a TUN/TAP interface to connect the Bluetooth network interface with the native system. It has been tested on Linux and OS X, but should work on any system that provides TUN/TAP with minor modifications.

 On Linux, TUN/TAP is available by default. On OS X, tuntaposx from http://tuntaposx.sourceforge.net needs to be installed.

 \emph{tap_alloc} sets up a virtual network interface with the given Bluetooth Address. It is rather low-level as it sets up and configures a network interface.

  [code snippet below](#panudemo:processTapData) shows how a packet is received from the TAP network interface and forwarded over the BNEP connection.

 After successfully reading a network packet, the call to \emph{bnep_can_send_packet_now} checks, if BTstack can forward a network packet now. If that's not possible, the received data stays in the network buffer and the data source elements is removed from the run loop. \emph{process_tap_dev_data} will not be called until the data source is registered again. This provides a basic flow control.


<a name="panudemo:processTapData"></a>
<!-- -->

    int process_tap_dev_data(struct data_source *ds) 
    {
      ssize_t len;
      len = read(ds->fd, network_buffer, sizeof(network_buffer));
      if (len <= 0){
        fprintf(stderr, "TAP: Error while reading: %s\n", strerror(errno));
        return 0;
      }
    
      network_buffer_len = len;
      if (bnep_can_send_packet_now(bnep_cid)) {
        bnep_send(bnep_cid, network_buffer, network_buffer_len);
        network_buffer_len = 0;
      } else {
        // park the current network packet
        run_loop_remove_data_source(&tap_dev_ds);
      }
      return 0;
    }




### SDP parser callback 


  The SDP parsers retrieves the BNEP PAN UUID as explained in Section  \ref{example:sdpbnepquery}.

### Packet Handler


  The packet handler responds to various HCI Events.


<a name="panudemo:packetHandler"></a>
<!-- -->

    static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
    {
    ...
      switch (packet_type) {
    		case HCI_EVENT_PACKET:
          event = packet[0];
          switch (event) {      
            case BTSTACK_EVENT_STATE:
              if (packet[2] == HCI_STATE_WORKING) {
                printf("Start SDP BNEP query.\n");
                sdp_general_query_for_uuid(remote, SDP_BNEPProtocol);
              }
              break;
    
    ...
    
            case BNEP_EVENT_INCOMING_CONNECTION:
    					// data: event(8), len(8), bnep source uuid (16), bnep destination uuid (16), remote_address (48)
              uuid_source = READ_BT_16(packet, 2);
              uuid_dest   = READ_BT_16(packet, 4);
              mtu     = READ_BT_16(packet, 6);
              bnep_cid  = channel;
              memcpy(&event_addr, &packet[8], sizeof(bd_addr_t));
    					printf("BNEP connection from %s source UUID 0x%04x dest UUID: 0x%04x, max frame size: %u\n", bd_addr_to_str(event_addr), uuid_source, uuid_dest, mtu);
              hci_local_bd_addr(local_addr);
              tap_fd = tap_alloc(tap_dev_name, local_addr);
              if (tap_fd < 0) {
                printf("Creating BNEP tap device failed: %s\n", strerror(errno));
              } else {
                printf("BNEP device \"%s\" allocated.\n", tap_dev_name);
                tap_dev_ds.fd = tap_fd;
                tap_dev_ds.process = process_tap_dev_data;
                run_loop_add_data_source(&tap_dev_ds);
              }
    					break;
    
    ...
    
            case BNEP_EVENT_CHANNEL_TIMEOUT:
              printf("BNEP channel timeout! Channel will be closed\n");
              break;
    
            case BNEP_EVENT_CHANNEL_CLOSED:
              printf("BNEP channel closed\n");
              run_loop_remove_data_source(&tap_dev_ds);
              if (tap_fd > 0) {
                close(tap_fd);
                tap_fd = -1;
              }
              break;
    
            case BNEP_EVENT_READY_TO_SEND:
              // Check for parked network packets and send it out now 
              if (network_buffer_len > 0) {
                bnep_send(bnep_cid, network_buffer, network_buffer_len);
                network_buffer_len = 0;
                // Re-add the tap device data source
                run_loop_add_data_source(&tap_dev_ds);
              }
              
              break;
              
            default:
              break;
          }
          break;
    
        case BNEP_DATA_PACKET:
          // Write out the ethernet frame to the tap device 
          if (tap_fd > 0) {
            rc = write(tap_fd, packet, size);
            if (rc < 0) {
              fprintf(stderr, "TAP: Could not write to TAP device: %s\n", strerror(errno));
            } else 
            if (rc != size) {
              fprintf(stderr, "TAP: Package written only partially %d of %d bytes\n", rc, size);
            }
          }
          break;      
          
        default:
          break;
      }
    }




  When BTSTACK_EVENT_STATE with state HCI_STATE_WORKING is received and the example is started in client mode, the remote SDP BNEP query is started.

  In server mode, BNEP_EVENT_INCOMING_CONNECTION is received after a client has connected. The TAP network interface is then configured. A data source is set up and registered with the  run loop to receive Ethernet packets from the TAP interface.

 The event contains both the source and destination UUIDs, as well as the MTU for this connection and the BNEP Channel ID, which is used for sending Ethernet packets over BNEP.

  In client mode, BNEP_EVENT_OPEN_CHANNEL_COMPLETE is received after a client has connected or when the connection fails. The status field returns the error code. It is otherwise identical to  BNEP_EVENT_INCOMING_CONNECTION before.

  If there is a timeout during the connection setup, BNEP_EVENT_CHANNEL_TIMEOUT will be received and the BNEP connection  will be closed

  BNEP_EVENT_CHANNEL_CLOSED is received when the connection gets closed.

  BNEP_EVENT_READY_TO_SEND indicates that a new packet can be send. This triggers the retry of a  parked network packet. If this succeeds, the data source element is added to the run loop again.

  Ethernet packets from the remote device are received in the packet handler with type BNEP_DATA_PACKET. It is forwarded to the TAP interface.

## gatt_browser: GATT Client - Discovering primary services and their characteristics
<a name="section:gattbrowser"></a>



  This example shows how to use the GATT Client API to discover primary services and their characteristics of the first found device that is advertising its services.

 The logic is divided between the HCI and GATT client packet handlers. The HCI packet handler is responsible for finding a remote device,  connecting to it, and for starting the first GATT client query. Then, the GATT client packet handler receives all primary services and requests the characteristics of the last one to keep the example short.



### GATT client setup


  In the setup phase, a GATT client must register the HCI and GATT client packet handlers, as shown in [code snippet below](#gattbrowser:GATTClientSetup). Additionally, the security manager can be setup, if signed writes, or encrypted, or authenticated connection are required, to access the characteristics, as explained in [here](#section:smp).


<a name="gattbrowser:GATTClientSetup"></a>
<!-- -->

    static uint16_t gc_id;
    
    // Handles connect, disconnect, and advertising report events,  
    // starts the GATT client, and sends the first query.
    static void handle_hci_event(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    
    // Handles GATT client query results, sends queries and the 
    // GAP disconnect command when the querying is done.
    void handle_gatt_client_event(le_event_t * event);
    
    static void gatt_client_setup(){
      // Initialize L2CAP and register HCI event handler
      l2cap_init();
      l2cap_register_packet_handler(&handle_hci_event);
    
      // Initialize GATT client 
      gatt_client_init();
      // Register handler for GATT client events
      gc_id = gatt_client_register_packet_handler(&handle_gatt_client_event);
    
      // Optinoally, Setup security manager
      sm_init();
      sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    }




### HCI packet handler


  The HCI packet handler has to start the scanning,  to find the first advertising device, to stop scanning, to connect to and later to disconnect from it, to start the GATT client upon the connection is completed, and to send the first query - in this case the gatt_client_discover_primary_services() is called, see  [code snippet below](#gattbrowser:GATTBrowserHCIPacketHandler).  


<a name="gattbrowser:GATTBrowserHCIPacketHandler"></a>
<!-- -->

    static void handle_hci_event(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
      if (packet_type != HCI_EVENT_PACKET) return;
      advertising_report_t report;
      
      uint8_t event = packet[0];
      switch (event) {
        case BTSTACK_EVENT_STATE:
          // BTstack activated, get started
          if (packet[2] != HCI_STATE_WORKING) break;
          if (cmdline_addr_found){
            printf("Trying to connect to %s\n", bd_addr_to_str(cmdline_addr));
            le_central_connect(cmdline_addr, 0);
            break;
          }
          printf("BTstack activated, start scanning!\n");
          le_central_set_scan_parameters(0,0x0030, 0x0030);
          le_central_start_scan();
          break;
        case GAP_LE_ADVERTISING_REPORT:
          fill_advertising_report_from_packet(&report, packet);
          // stop scanning, and connect to the device
          le_central_stop_scan();
          le_central_connect(report.address,report.address_type);
          break;
        case HCI_EVENT_LE_META:
          // wait for connection complete
          if (packet[2] !=  HCI_SUBEVENT_LE_CONNECTION_COMPLETE) break;
          gc_handle = READ_BT_16(packet, 4);
          // query primary services
          gatt_client_discover_primary_services(gc_id, gc_handle);
          break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
          printf("\nGATT browser - DISCONNECTED\n");
          exit(0);
          break;
        default:
          break;
      }
    }




### GATT Client event handler


  Query results and further queries are handled by the GATT client packet handler, as shown in [code snippet below](#gattbrowser:GATTBrowserQueryHandler). Here, upon receiving the primary services, the gatt_client_discover_characteristics_for_service() query for the last received service is sent. After receiving the characteristics for the service, gap_disconnect is called to terminate the connection. Upon disconnect, the HCI packet handler receives the disconnect complete event.  


<a name="gattbrowser:GATTBrowserQueryHandler"></a>
<!-- -->

    static int search_services = 1;
    
    void handle_gatt_client_event(le_event_t * event){
      le_service_t service;
      le_characteristic_t characteristic;
      switch(event->type){
        case GATT_SERVICE_QUERY_RESULT:
          service = ((le_service_event_t *) event)->service;
          dump_service(&service);
          services[service_count++] = service;
          break;
        case GATT_CHARACTERISTIC_QUERY_RESULT:
          characteristic = ((le_characteristic_event_t *) event)->characteristic;
          dump_characteristic(&characteristic);
          break;
        case GATT_QUERY_COMPLETE:
          if (search_services){
            // GATT_QUERY_COMPLETE of search services 
            service_index = 0;
            printf("\nGATT browser - CHARACTERISTIC for SERVICE ");
            printUUID128(service.uuid128); printf("\n");
            search_services = 0;
            gatt_client_discover_characteristics_for_service(gc_id, gc_handle, &services[service_index]);
          } else {
            // GATT_QUERY_COMPLETE of search characteristics
            if (service_index < service_count) {
              service = services[service_index++];
              printf("\nGATT browser - CHARACTERISTIC for SERVICE ");
              printUUID128(service.uuid128);
              printf(", [0x%04x-0x%04x]\n", service.start_group_handle, service.end_group_handle);
              
              gatt_client_discover_characteristics_for_service(gc_id, gc_handle, &service);
              break;
            }
            service_index = 0;
            gap_disconnect(gc_handle); 
          }
          break;
        default:
          break;
      }
    }




## le_counter: LE Peripheral - Heartbeat Counter over GATT
<a name="section:lecounter"></a>



  All newer operating systems provide GATT Client functionality. The LE Counter examples demonstrates how to specify a minimal GATT Database with a custom GATT Service and a custom Characteristic that sends periodic notifications. 

### Main Application Setup


  [code snippet below](#lecounter:MainConfiguration) shows main application code. It initializes L2CAP, the Security Manager and configures the ATT Server with the pre-compiled ATT Database generated from $le_counter.gatt$. Finally, it configures the heartbeat handler and boots the Bluetooth stack.


<a name="lecounter:MainConfiguration"></a>
<!-- -->

    static int  le_notification_enabled;
    static timer_source_t heartbeat;
    
    static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static uint16_t att_read_callback(uint16_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
    static int att_write_callback(uint16_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
    static void  heartbeat_handler(struct timer *ts);
    
    static void le_counter_setup(){
      l2cap_init();
      l2cap_register_packet_handler(packet_handler);
    
      // setup le device db
      le_device_db_init();
    
      // setup SM: Display only
      sm_init();
    
      // setup ATT server
      att_server_init(profile_data, att_read_callback, att_write_callback);  
      att_dump_attributes();
      // set one-shot timer
      heartbeat.process = &heartbeat_handler;
      run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
      run_loop_add_timer(&heartbeat);
    }




### Managing LE Advertisements


  To be discoverable, LE Advertisements are enabled using direct HCI Commands. As there's no guarantee that a HCI command can always be sent, the gap_run function is used to send each command as requested by the $todos$ variable. First, the advertisement data is set with hci_le_set_advertising_data. Then the advertisement parameters including the  advertisement interval is set with hci_le_set_advertising_parameters. Finally, advertisements are enabled with hci_le_set_advertise_enable. See [code snippet below](#lecounter:gapRun).

 In this example, the Advertisement contains the Flags attribute and the device name. The flag 0x06 indicates: LE General Discoverable Mode and BR/EDR not supported.


<a name="lecounter:gapRun"></a>
<!-- -->

    const uint8_t adv_data[] = {
      // Flags general discoverable
      0x02, 0x01, 0x06, 
      // Name
      0x0b, 0x09, 'L', 'E', ' ', 'C', 'o', 'u', 'n', 't', 'e', 'r', 
    };
    
    uint8_t adv_data_len = sizeof(adv_data);
    enum {
      SET_ADVERTISEMENT_PARAMS = 1 << 0,
      SET_ADVERTISEMENT_DATA   = 1 << 1,
      ENABLE_ADVERTISEMENTS  = 1 << 2,
    };
    static uint16_t todos = 0;
    
    static void gap_run(){
    
      if (!hci_can_send_command_packet_now()) return;
    
      if (todos & SET_ADVERTISEMENT_DATA){
        printf("GAP_RUN: set advertisement data\n");
        todos &= ~SET_ADVERTISEMENT_DATA;
        hci_send_cmd(&hci_le_set_advertising_data, adv_data_len, adv_data);
        return;
      }  
    
      if (todos & SET_ADVERTISEMENT_PARAMS){
        todos &= ~SET_ADVERTISEMENT_PARAMS;
        uint8_t adv_type = 0;   // default
        bd_addr_t null_addr;
        memset(null_addr, 0, 6);
        uint16_t adv_int_min = 0x0030;
        uint16_t adv_int_max = 0x0030;
        hci_send_cmd(&hci_le_set_advertising_parameters, adv_int_min, adv_int_max, adv_type, 0, 0, &null_addr, 0x07, 0x00);
        return;
      }  
    
      if (todos & ENABLE_ADVERTISEMENTS){
        printf("GAP_RUN: enable advertisements\n");
        todos &= ~ENABLE_ADVERTISEMENTS;
        hci_send_cmd(&hci_le_set_advertise_enable, 1);
        return;
      }
    }




### Packet Handler


  The packet handler is only used to trigger advertisements  after BTstack is ready and after disconnects, see [code snippet below](#lecounter:packetHandler).  Advertisements are not automatically re-enabled after a connection  was closed, even though they have been active before.


<a name="lecounter:packetHandler"></a>
<!-- -->

    static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    	switch (packet_type) {
    		case HCI_EVENT_PACKET:
    			switch (packet[0]) {
    					
    				case BTSTACK_EVENT_STATE:
    					// BTstack activated, get started 
    					if (packet[2] == HCI_STATE_WORKING) {
                todos = SET_ADVERTISEMENT_PARAMS | SET_ADVERTISEMENT_DATA | ENABLE_ADVERTISEMENTS;
                gap_run();
    					}
    					break;
    
            case HCI_EVENT_DISCONNECTION_COMPLETE:
              todos = ENABLE_ADVERTISEMENTS;
              le_notification_enabled = 0;
              gap_run();
              break;
            
            default:
              break;
    			}
          break;
    
        default:
          break;
    	}
      gap_run();
    }




### Heartbeat Handler


  The heartbeat handler updates the value of the single Characteristic provided in this example, and sends a notification for this characteristic if enabled, see [code snippet below](#lecounter:heartbeat).


<a name="lecounter:heartbeat"></a>
<!-- -->

    static int  counter = 0;
    static char counter_string[30];
    static int  counter_string_len;
    
    static void  heartbeat_handler(struct timer *ts){
      counter++;
      counter_string_len = sprintf(counter_string, "BTstack counter %04u\n", counter);
      puts(counter_string);
    
      if (le_notification_enabled) {
        att_server_notify(ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) counter_string, counter_string_len);
      }
      run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
      run_loop_add_timer(ts);
    } 




### ATT Read


  The ATT Server handles all reads to constant data. For dynamic data like the custom characteristic, the registered att_read_callback is called. To handle long characteristics and long reads, the att_read_callback is first called with buffer == NULL, to request the total value length. Then it will be called again requesting a chunk of the value. See [code snippet below](#lecounter:attRead).


<a name="lecounter:attRead"></a>
<!-- -->

    
    // ATT Client Read Callback for Dynamic Data
    // - if buffer == NULL, don't copy data, just return size of value
    // - if buffer != NULL, copy data and return number bytes copied
    // @param offset defines start of attribute value
    static uint16_t att_read_callback(uint16_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
      if (att_handle == ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE){
        if (buffer){
          memcpy(buffer, &counter_string[offset], counter_string_len - offset);
        }
        return counter_string_len - offset;
      }
      return 0;
    }




### ATT Write


  The only valid ATT write in this example is to the Client Characteristic Configuration, which configures notification and indication. If the ATT handle matches the client configuration handle, the new configuration value is stored and used in the heartbeat handler to decide if a new value should be sent. See [code snippet below](#lecounter:attWrite).


<a name="lecounter:attWrite"></a>
<!-- -->

    static int att_write_callback(uint16_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
      if (att_handle != ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE) return 0;
      le_notification_enabled = READ_BT_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
      return 0;
    }




## spp_and_le_counter: Dual mode example
<a name="section:sppandlecounter"></a>



  The SPP and LE Counter example combines the Bluetooth Classic SPP Counter and the Bluetooth LE Counter into a single application.

 In this Section, we only point out the differences to the individual examples and how how the stack is configured.

### Advertisements 


  The Flags attribute in the Advertisement Data indicates if a device is in dual-mode or not. Flag 0x02 indicates LE General Discoverable, Dual-Mode device. See [code snippet below](#sppandlecounter:advertisements).


<a name="sppandlecounter:advertisements"></a>
<!-- -->

    const uint8_t adv_data[] = {
      // Flags: General Discoverable
      0x02, 0x01, 0x02, 
      // Name
      0x0b, 0x09, 'L', 'E', ' ', 'C', 'o', 'u', 'n', 't', 'e', 'r', 
    };




### Packet Handler


  The packet handler of the combined example is just the combination of the individual packet handlers.

### Heartbeat Handler


  Similar to the packet handler, the heartbeat handler is the combination of the individual ones. After updating the counter, it sends an RFCOMM packet if an RFCOMM connection is active, and an LE notification if the remote side has requested notifications.


<a name="sppandlecounter:heartbeat"></a>
<!-- -->

    static void  heartbeat_handler(struct timer *ts){
    
      counter++;
      counter_string_len = sprintf(counter_string, "BTstack counter %04u\n", counter);
      // printf("%s", counter_string);
    
      if (rfcomm_channel_id){
        if (rfcomm_can_send_packet_now(rfcomm_channel_id)){
          int err = rfcomm_send_internal(rfcomm_channel_id, (uint8_t*) counter_string, counter_string_len);
          if (err) {
            log_error("rfcomm_send_internal -> error 0X%02x", err);
          }
        }
      }
    
      if (le_notification_enabled) {
        att_server_notify(ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) counter_string, counter_string_len);
      }
      run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
      run_loop_add_timer(ts);
    } 




### Main Application Setup


  As with the packet and the heartbeat handlers, the combined app setup contains the code from the individual example setups.


<a name="sppandlecounter:MainConfiguration"></a>
<!-- -->

    int btstack_main(void);
    int btstack_main(void)
    {
      hci_discoverable_control(1);
    
      l2cap_init();
      l2cap_register_packet_handler(packet_handler);
    
      rfcomm_init();
      rfcomm_register_packet_handler(packet_handler);
      rfcomm_register_service_internal(NULL, RFCOMM_SERVER_CHANNEL, 0xffff);
    
      // init SDP, create record for SPP and register with SDP
      sdp_init();
      memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    ...
      service_record_item_t * service_record_item = (service_record_item_t *) spp_service_buffer;
      sdp_create_spp_service( (uint8_t*) &service_record_item->service_record, RFCOMM_SERVER_CHANNEL, "SPP Counter");
      printf("SDP service buffer size: %u\n", (uint16_t) (sizeof(service_record_item_t) + de_get_len((uint8_t*) &service_record_item->service_record)));
      sdp_register_service_internal(NULL, service_record_item);
    ...
    
      hci_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    
      // setup le device db
      le_device_db_init();
    
      // setup SM: Display only
      sm_init();
    
      // setup ATT server
      att_server_init(profile_data, att_read_callback, att_write_callback);  
      att_dump_attributes();
      // set one-shot timer
      heartbeat.process = &heartbeat_handler;
      run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
      run_loop_add_timer(&heartbeat);
    
      // turn on!
    	hci_power_control(HCI_POWER_ON);
    	  
      return 0;
    }


