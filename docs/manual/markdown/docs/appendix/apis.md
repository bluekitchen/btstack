

## Run Loop API
<a name ="appendix:api_run_loop"></a>

    /**
     * @brief Set timer based on current time in milliseconds.
     */
    void run_loop_set_timer(timer_source_t *a, uint32_t timeout_in_ms);
    
    /**
     * @brief Set callback that will be executed when timer expires.
     */
    void run_loop_set_timer_handler(timer_source_t *ts, void (*process)(timer_source_t *_ts));
    
    /**
     * @brief Add/Remove timer source.
     */
    void run_loop_add_timer(timer_source_t *timer); 
    int  run_loop_remove_timer(timer_source_t *timer);
    
    /**
     * @brief Init must be called before any other run_loop call. Use RUN_LOOP_EMBEDDED for embedded devices.
     */
    void run_loop_init(RUN_LOOP_TYPE type);
    
    /**
     * @brief Set data source callback.
     */
    void run_loop_set_data_source_handler(data_source_t *ds, int (*process)(data_source_t *_ds));
    
    /**
     * @brief Add/Remove data source.
     */
    void run_loop_add_data_source(data_source_t *dataSource);
    int  run_loop_remove_data_source(data_source_t *dataSource);
    
    /**
     * @brief Execute configured run loop. This function does not return.
     */
    void run_loop_execute(void);
    
    // hack to fix HCI timer handling
    #ifdef HAVE_TICK
    /**
     * @brief Sets how many milliseconds has one tick.
     */
    uint32_t embedded_ticks_for_ms(uint32_t time_in_ms);
    /**
     * @brief Queries the current time in ticks.
     */
    uint32_t embedded_get_ticks(void);
    /**
     * @brief Queries the current time in ms
     */
    uint32_t embedded_get_time_ms(void);
    /**
     * @brief Allows to update BTstack system ticks based on another already existing clock.
     */
    void embedded_set_ticks(uint32_t ticks);
    #endif
    #ifdef EMBEDDED
    /**
     * @brief Sets an internal flag that is checked in the critical section just before entering sleep mode. Has to be called by the interrupt handler of a data source to signal the run loop that a new data is available.
     */
    void embedded_trigger(void);    
    /**
     * @brief Execute run_loop once. It can be used to integrate BTstack's timer and data source processing into a foreign run loop (it is not recommended).
     */
    void embedded_execute_once(void);
    #endif


## HCI API
<a name ="appendix:api_hci"></a>

    le_connection_parameter_range_t gap_le_get_connection_parameter_range();
    void gap_le_set_connection_parameter_range(le_connection_parameter_range_t range);
    
    /* LE Client Start */
    
    le_command_status_t le_central_start_scan(void);
    le_command_status_t le_central_stop_scan(void);
    le_command_status_t le_central_connect(bd_addr_t addr, bd_addr_type_t addr_type);
    le_command_status_t le_central_connect_cancel(void);
    le_command_status_t gap_disconnect(hci_con_handle_t handle);
    void le_central_set_scan_parameters(uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window);
    
    /* LE Client End */
        
    void hci_connectable_control(uint8_t enable);
    void hci_close(void);
    
    /** 
     * @note New functions replacing: hci_can_send_packet_now[_using_packet_buffer]
     */
    int hci_can_send_command_packet_now(void);
        
    /**
     * @brief Gets local address.
     */
    void hci_local_bd_addr(bd_addr_t address_buffer);
    
    /**
     * @brief Set up HCI. Needs to be called before any other function.
     */
    void hci_init(hci_transport_t *transport, void *config, bt_control_t *control, remote_device_db_t const* remote_device_db);
    
    /**
     * @brief Set class of device that will be set during Bluetooth init.
     */
    void hci_set_class_of_device(uint32_t class_of_device);
    
    /**
     * @brief Set Public BD ADDR - passed on to Bluetooth chipset if supported in bt_control_h
     */
    void hci_set_bd_addr(bd_addr_t addr);
    
    /**
     * @brief Registers a packet handler. Used if L2CAP is not used (rarely). 
     */
    void hci_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size));
    
    /**
     * @brief Requests the change of BTstack power mode.
     */
    int  hci_power_control(HCI_POWER_MODE mode);
    
    /**
     * @brief Allows to control if device is discoverable. OFF by default.
     */
    void hci_discoverable_control(uint8_t enable);
    
    /**
     * @brief Creates and sends HCI command packets based on a template and a list of parameters. Will return error if outgoing data buffer is occupied. 
     */
    int hci_send_cmd(const hci_cmd_t *cmd, ...);
    
    /**
     * @brief Deletes link key for remote device with baseband address.
     */
    void hci_drop_link_key_for_bd_addr(bd_addr_t addr);
    
    /* Configure Secure Simple Pairing */
    
    /**
     * @brief Enable will enable SSP during init.
     */
    void hci_ssp_set_enable(int enable);
    
    /**
     * @brief If set, BTstack will respond to io capability request using authentication requirement.
     */
    void hci_ssp_set_io_capability(int ssp_io_capability);
    void hci_ssp_set_authentication_requirement(int authentication_requirement);
    
    /**
     * @brief If set, BTstack will confirm a numeric comparison and enter '000000' if requested.
     */
    void hci_ssp_set_auto_accept(int auto_accept);
    
    /**
     * @brief Get addr type and address used in advertisement packets.
     */
    void hci_le_advertisement_address(uint8_t * addr_type, bd_addr_t addr);


## L2CAP API
<a name ="appendix:api_l2cap"></a>

    /** 
     * @brief Set up L2CAP and register L2CAP with HCI layer.
     */
    void l2cap_init(void);
    
    /** 
     * @brief Registers a packet handler that handles HCI and general BTstack events.
     */
    void l2cap_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size));
    
    /** 
     * @brief Creates L2CAP channel to the PSM of a remote device with baseband address. A new baseband connection will be initiated if necessary.
     */
    void l2cap_create_channel_internal(void * connection, btstack_packet_handler_t packet_handler, bd_addr_t address, uint16_t psm, uint16_t mtu);
    
    /** 
     * @brief Disconnects L2CAP channel with given identifier. 
     */
    void l2cap_disconnect_internal(uint16_t local_cid, uint8_t reason);
    
    /** 
     * @brief Queries the maximal transfer unit (MTU) for L2CAP channel with given identifier. 
     */
    uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid);
    
    /** 
     * @brief Sends L2CAP data packet to the channel with given identifier.
     */
    int l2cap_send_internal(uint16_t local_cid, uint8_t *data, uint16_t len);
    
    /** 
     * @brief Registers L2CAP service with given PSM and MTU, and assigns a packet handler. On embedded systems, use NULL for connection parameter.
     */
    void l2cap_register_service_internal(void *connection, btstack_packet_handler_t packet_handler, uint16_t psm, uint16_t mtu, gap_security_level_t security_level);
    
    /** 
     * @brief Unregisters L2CAP service with given PSM.  On embedded systems, use NULL for connection parameter.
     */
    void l2cap_unregister_service_internal(void *connection, uint16_t psm);
    
    /** 
     * @brief Accepts/Deny incoming L2CAP connection.
     */
    void l2cap_accept_connection_internal(uint16_t local_cid);
    void l2cap_decline_connection_internal(uint16_t local_cid, uint8_t reason);
    
    /** 
     * @brief Request LE connection parameter update
     */
    int l2cap_le_request_connection_parameter_update(uint16_t handle, uint16_t interval_min, uint16_t interval_max, uint16_t slave_latency, uint16_t timeout_multiplier);
    
    /** 
     * @brief Non-blocking UART write
     */
    int  l2cap_can_send_packet_now(uint16_t local_cid);    
    int  l2cap_reserve_packet_buffer(void);
    void l2cap_release_packet_buffer(void);
    
    /** 
     * @brief Get outgoing buffer and prepare data.
     */
    uint8_t *l2cap_get_outgoing_buffer(void);
    
    int l2cap_send_prepared(uint16_t local_cid, uint16_t len);
    
    int l2cap_send_prepared_connectionless(uint16_t handle, uint16_t cid, uint16_t len);
    
    /** 
     * @brief Bluetooth 4.0 - allows to register handler for Attribute Protocol and Security Manager Protocol.
     */
    void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id);
    
    uint16_t l2cap_max_mtu(void);
    uint16_t l2cap_max_le_mtu(void);
    
    int  l2cap_send_connectionless(uint16_t handle, uint16_t cid, uint8_t *data, uint16_t len);


## RFCOMM API
<a name ="appendix:api_rfcomm"></a>

    /** 
     * @brief Set up RFCOMM.
     */
    void rfcomm_init(void);
    
    /** 
     * @brief Set security level required for incoming connections, need to be called before registering services.
     */
    void rfcomm_set_required_security_level(gap_security_level_t security_level);
    
    /** 
     * @brief Register packet handler.
     */
    void rfcomm_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size));
    
    /** 
     * @brief Creates RFCOMM connection (channel) to a given server channel on a remote device with baseband address. A new baseband connection will be initiated if necessary. This channel will automatically provide enough credits to the remote side
     */
    void rfcomm_create_channel_internal(void * connection, bd_addr_t addr, uint8_t channel);
    
    /** 
     * @brief Creates RFCOMM connection (channel) to a given server channel on a remote device with baseband address. new baseband connection will be initiated if necessary. This channel will use explicit credit management. During channel establishment, an initial  amount of credits is provided.
     */
    void rfcomm_create_channel_with_initial_credits_internal(void * connection, bd_addr_t addr, uint8_t server_channel, uint8_t initial_credits);
    
    /** 
     * @brief Disconnects RFCOMM channel with given identifier. 
     */
    void rfcomm_disconnect_internal(uint16_t rfcomm_cid);
    
    /** 
     * @brief Registers RFCOMM service for a server channel and a maximum frame size, and assigns a packet handler. On embedded systems, use NULL for connection parameter. This channel provides automatically enough credits to the remote side.
     */
    void rfcomm_register_service_internal(void * connection, uint8_t channel, uint16_t max_frame_size);
    
    /** 
     * @brief Registers RFCOMM service for a server channel and a maximum frame size, and assigns a packet handler. On embedded systems, use NULL for connection parameter. This channel will use explicit credit management. During channel establishment, an initial amount of credits is provided.
     */
    void rfcomm_register_service_with_initial_credits_internal(void * connection, uint8_t channel, uint16_t max_frame_size, uint8_t initial_credits);
    
    /** 
     * @brief Unregister RFCOMM service.
     */
    void rfcomm_unregister_service_internal(uint8_t service_channel);
    
    /** 
     * @brief Accepts/Deny incoming RFCOMM connection.
     */
    void rfcomm_accept_connection_internal(uint16_t rfcomm_cid);
    void rfcomm_decline_connection_internal(uint16_t rfcomm_cid);
    
    /** 
     * @brief Grant more incoming credits to the remote side for the given RFCOMM channel identifier.
     */
    void rfcomm_grant_credits(uint16_t rfcomm_cid, uint8_t credits);
    
    /** 
     * @brief Checks if RFCOMM can send packet. Returns yes if packet can be sent.
     */
    int rfcomm_can_send_packet_now(uint16_t rfcomm_cid);
    
    /** 
     * @brief Sends RFCOMM data packet to the RFCOMM channel with given identifier.
     */
    int  rfcomm_send_internal(uint16_t rfcomm_cid, uint8_t *data, uint16_t len);
    
    /** 
     * @brief Sends Local Line Status, see LINE_STATUS_..
     */
    int rfcomm_send_local_line_status(uint16_t rfcomm_cid, uint8_t line_status);
    
    /** 
     * @brief Send local modem status. see MODEM_STAUS_..
     */
    int rfcomm_send_modem_status(uint16_t rfcomm_cid, uint8_t modem_status);
    
    /** 
     * @brief Configure remote port 
     */
    int rfcomm_send_port_configuration(uint16_t rfcomm_cid, rpn_baud_t baud_rate, rpn_data_bits_t data_bits, rpn_stop_bits_t stop_bits, rpn_parity_t parity, rpn_flow_control_t flow_control);
    
    /** 
     * @brief Query remote port 
     */
    int rfcomm_query_port_configuration(uint16_t rfcomm_cid);
    
    /** 
     * @brief Allow to create RFCOMM packet in outgoing buffer.
     */
    int       rfcomm_reserve_packet_buffer(void);
    void      rfcomm_release_packet_buffer(void);
    uint8_t * rfcomm_get_outgoing_buffer(void);
    uint16_t  rfcomm_get_max_frame_size(uint16_t rfcomm_cid);
    int       rfcomm_send_prepared(uint16_t rfcomm_cid, uint16_t len);


## SDP API
<a name ="appendix:api_sdp"></a>

    /** 
     * @brief Set up SDP.
     */
    void sdp_init(void);
    
    void sdp_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size));
    
    #ifdef EMBEDDED
    /** 
     * @brief Register service record internally - this version doesn't copy the record therefore it must be forever accessible. Preconditions:
        - AttributeIDs are in ascending order;
        - ServiceRecordHandle is first attribute and valid.
     * @return ServiceRecordHandle or 0 if registration failed.
     */
    uint32_t sdp_register_service_internal(void *connection, service_record_item_t * record_item);
    #endif
    
    #ifndef EMBEDDED
    /** 
     * @brief Register service record internally - this version creates a copy of the record precondition: AttributeIDs are in ascending order => ServiceRecordHandle is first attribute if present. 
     * @return ServiceRecordHandle or 0 if registration failed
     */
    uint32_t sdp_register_service_internal(void *connection, uint8_t * service_record);
    #endif
    
    /** 
     * @brief Unregister service record internally.
     */
    void sdp_unregister_service_internal(void *connection, uint32_t service_record_handle);


## SDP Client API
<a name ="appendix:api_sdp_client"></a>

     
    /** 
     * @brief Queries the SDP service of the remote device given a service search pattern and a list of attribute IDs. The remote data is handled by the SDP parser. The SDP parser delivers attribute values and done event via a registered callback.
     */
    void sdp_client_query(bd_addr_t remote, uint8_t * des_serviceSearchPattern, uint8_t * des_attributeIDList);
    
    #ifdef HAVE_SDP_EXTRA_QUERIES
    void sdp_client_service_attribute_search(bd_addr_t remote, uint32_t search_serviceRecordHandle, uint8_t * des_attributeIDList);
    void sdp_client_service_search(bd_addr_t remote, uint8_t * des_serviceSearchPattern);
    #endif


## SDP RFCOMM Query API
<a name ="appendix:api_sdp_queries"></a>

    /** 
     * @brief SDP Query RFCOMM event to deliver channel number and service name byte by byte.
     */
    typedef struct sdp_query_rfcomm_service_event {
        uint8_t type;
        uint8_t channel_nr;
        uint8_t * service_name;
    } sdp_query_rfcomm_service_event_t;
    
    /** 
     * @brief Registers a callback to receive RFCOMM service and query complete event. 
     */
    void sdp_query_rfcomm_register_callback(void(*sdp_app_callback)(sdp_query_event_t * event, void * context), void * context);
    
    void sdp_query_rfcomm_deregister_callback(void);
    
    /** 
     * @brief Searches SDP records on a remote device for RFCOMM services with a given UUID.
     */
    void sdp_query_rfcomm_channel_and_name_for_uuid(bd_addr_t remote, uint16_t uuid);
    
    /** 
     * @brief Searches SDP records on a remote device for RFCOMM services with a given service search pattern.
     */
    void sdp_query_rfcomm_channel_and_name_for_search_pattern(bd_addr_t remote, uint8_t * des_serviceSearchPattern);


## GATT Client API
<a name ="appendix:api_gatt_client"></a>

    typedef struct gatt_complete_event{
        uint8_t   type;
        uint16_t handle;
        uint16_t attribute_handle;
        uint8_t status;
    } gatt_complete_event_t;
    
    typedef struct le_service{
        uint16_t start_group_handle;
        uint16_t end_group_handle;
        uint16_t uuid16;
        uint8_t  uuid128[16];
    } le_service_t;
    
    typedef struct le_service_event{
        uint8_t  type;
        uint16_t handle;
        le_service_t service;
    } le_service_event_t;
    
    typedef struct le_characteristic{
        uint16_t start_handle;
        uint16_t value_handle;
        uint16_t end_handle;
        uint16_t properties;
        uint16_t uuid16;
        uint8_t  uuid128[16];
    } le_characteristic_t;
    
    typedef struct le_characteristic_event{
        uint8_t  type;
        uint16_t handle;
        le_characteristic_t characteristic;
    } le_characteristic_event_t;
    
    typedef struct le_characteristic_value_event{
        uint8_t   type;
        uint16_t  handle;
        uint16_t  value_handle;
        uint16_t  value_offset;
        uint16_t  blob_length;
        uint8_t * blob;
    } le_characteristic_value_event_t;
    
    typedef struct le_characteristic_descriptor{
        uint16_t handle;
        uint16_t uuid16;
        uint8_t  uuid128[16];
    } le_characteristic_descriptor_t;
    
    typedef struct le_characteristic_descriptor_event{
        uint8_t  type;
        uint16_t handle;
        le_characteristic_descriptor_t characteristic_descriptor;
        uint16_t value_length;
        uint16_t value_offset;
        uint8_t * value;
    } le_characteristic_descriptor_event_t;
    
    /** 
     * @brief Set up GATT client.
     */
    void gatt_client_init(void);
    
    /** 
     * @brief Register callback (packet handler) for GATT client. Returns GATT client ID.
     */
    uint16_t gatt_client_register_packet_handler (gatt_client_callback_t callback);
    
    /** 
     * @brief Unregister callback (packet handler) for GATT client.
     */
    void gatt_client_unregister_packet_handler(uint16_t gatt_client_id);
    
    /** 
     * @brief MTU is available after the first query has completed. If status is equal to BLE_PERIPHERAL_OK, it returns the real value, otherwise the default value of 23. 
     */
    le_command_status_t gatt_client_get_mtu(uint16_t handle, uint16_t * mtu);
    
    /** 
     * @brief Returns if the GATT client is ready to receive a query. It is used with daemon. 
     */
    int gatt_client_is_ready(uint16_t handle);
    
    /** 
     * @brief Discovers all primary services. For each found service, an le_service_event_t with type set to GATT_SERVICE_QUERY_RESULT will be generated and passed to the registered callback. The gatt_complete_event_t, with type set to GATT_QUERY_COMPLETE, marks the end of discovery. 
     */
    le_command_status_t gatt_client_discover_primary_services(uint16_t gatt_client_id, uint16_t con_handle);
    
    /** 
     * @brief Discovers a specific primary service given its UUID. This service may exist multiple times. For each found service, an le_service_event_t with type set to GATT_SERVICE_QUERY_RESULT will be generated and passed to the registered callback. The gatt_complete_event_t, with type set to GATT_QUERY_COMPLETE, marks the end of discovery. 
     */
    le_command_status_t gatt_client_discover_primary_services_by_uuid16(uint16_t gatt_client_id, uint16_t con_handle, uint16_t uuid16);
    le_command_status_t gatt_client_discover_primary_services_by_uuid128(uint16_t gatt_client_id, uint16_t con_handle, const uint8_t  * uuid);
    
    /** 
     * @brief Finds included services within the specified service. For each found included service, an le_service_event_t with type set to GATT_INCLUDED_SERVICE_QUERY_RESULT will be generated and passed to the registered callback. The gatt_complete_event_t with type set to GATT_QUERY_COMPLETE, marks the end of discovery. Information about included service type (primary/secondary) can be retrieved either by sending an ATT find information request for the returned start group handle (returning the handle and the UUID for primary or secondary service) or by comparing the service to the list of all primary services. 
     */
    le_command_status_t gatt_client_find_included_services_for_service(uint16_t gatt_client_id, uint16_t con_handle, le_service_t  *service);
    
    /** 
     * @brief Discovers all characteristics within the specified service. For each found characteristic, an le_characteristics_event_t with type set to GATT_CHARACTERISTIC_QUERY_RESULT will be generated and passed to the registered callback. The gatt_complete_event_t with type set to GATT_QUERY_COMPLETE, marks the end of discovery.
     */
    le_command_status_t gatt_client_discover_characteristics_for_service(uint16_t gatt_client_id, uint16_t con_handle, le_service_t  *service);
    
    /** 
     * @brief The following four functions are used to discover all characteristics within the specified service or handle range, and return those that match the given UUID. For each found characteristic, an le_characteristic_event_t with type set to GATT_CHARACTERISTIC_QUERY_RESULT will be generated and passed to the registered callback. The gatt_complete_event_t with type set to GATT_QUERY_COMPLETE, marks the end of discovery.
     */
    le_command_status_t gatt_client_discover_characteristics_for_handle_range_by_uuid16(uint16_t gatt_client_id, uint16_t con_handle, uint16_t start_handle, uint16_t end_handle, uint16_t uuid16);
    le_command_status_t gatt_client_discover_characteristics_for_handle_range_by_uuid128(uint16_t gatt_client_id, uint16_t con_handle, uint16_t start_handle, uint16_t end_handle, uint8_t  * uuid);
    le_command_status_t gatt_client_discover_characteristics_for_service_by_uuid16 (uint16_t gatt_client_id, uint16_t con_handle, le_service_t  *service, uint16_t  uuid16);
    le_command_status_t gatt_client_discover_characteristics_for_service_by_uuid128(uint16_t gatt_client_id, uint16_t con_handle, le_service_t  *service, uint8_t  * uuid128);
    
    /** 
     * @brief Discovers attribute handle and UUID of a characteristic descriptor within the specified characteristic. For each found descriptor, an le_characteristic_descriptor_event_t with type set to GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT will be generated and passed to the registered callback. The gatt_complete_event_t with type set to GATT_QUERY_COMPLETE, marks the end of discovery.
     */
    le_command_status_t gatt_client_discover_characteristic_descriptors(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_t  *characteristic);
    
    /** 
     * @brief Reads the characteristic value using the characteristic's value handle. If the characteristic value is found, an le_characteristic_value_event_t with type set to GATT_CHARACTERISTIC_VALUE_QUERY_RESULT will be generated and passed to the registered callback. The gatt_complete_event_t with type set to GATT_QUERY_COMPLETE, marks the end of read.
     */
    le_command_status_t gatt_client_read_value_of_characteristic(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_t  *characteristic);
    le_command_status_t gatt_client_read_value_of_characteristic_using_value_handle(uint16_t gatt_client_id, uint16_t con_handle, uint16_t characteristic_value_handle);
    
    /** 
     * @brief Reads the long characteristic value using the characteristic's value handle. The value will be returned in several blobs. For each blob, an le_characteristic_value_event_t with type set to GATT_CHARACTERISTIC_VALUE_QUERY_RESULT and updated value offset will be generated and passed to the registered callback. The gatt_complete_event_t with type set to GATT_QUERY_COMPLETE, mark the end of read.
     */
    le_command_status_t gatt_client_read_long_value_of_characteristic(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_t  *characteristic);
    le_command_status_t gatt_client_read_long_value_of_characteristic_using_value_handle(uint16_t gatt_client_id, uint16_t con_handle, uint16_t characteristic_value_handle);
    
    /** 
     * @brief Writes the characteristic value using the characteristic's value handle without an acknowledgment that the write was successfully performed.
     */
    le_command_status_t gatt_client_write_value_of_characteristic_without_response(uint16_t gatt_client_id, uint16_t con_handle, uint16_t characteristic_value_handle, uint16_t length, uint8_t  * data);
    
    /** 
     * @brief Writes the authenticated characteristic value using the characteristic's value handle without an acknowledgment that the write was successfully performed.
     */
    le_command_status_t gatt_client_signed_write_without_response(uint16_t gatt_client_id, uint16_t con_handle, uint16_t handle, uint16_t message_len, uint8_t  * message);
    
    /** 
     * @brief Writes the characteristic value using the characteristic's value handle. The gatt_complete_event_t with type set to GATT_QUERY_COMPLETE, marks the end of write. The write is successfully performed, if the event's status field is set to 0.
     */
    le_command_status_t gatt_client_write_value_of_characteristic(uint16_t gatt_client_id, uint16_t con_handle, uint16_t characteristic_value_handle, uint16_t length, uint8_t  * data);
    le_command_status_t gatt_client_write_long_value_of_characteristic(uint16_t gatt_client_id, uint16_t con_handle, uint16_t characteristic_value_handle, uint16_t length, uint8_t  * data);
    
    /** 
     * @brief Writes of the long characteristic value using the characteristic's value handle. It uses server response to validate that the write was correctly received. The gatt_complete_event_t with type set to GATT_QUERY_COMPLETE marks the end of write. The write is successfully performed, if the event's status field is set to 0.
     */
    le_command_status_t gatt_client_reliable_write_long_value_of_characteristic(uint16_t gatt_client_id, uint16_t con_handle, uint16_t characteristic_value_handle, uint16_t length, uint8_t  * data);
    
    /** 
     * @brief Reads the characteristic descriptor using its handle. If the characteristic descriptor is found, an le_characteristic_descriptor_event_t with type set to GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT will be generated and passed to the registered callback. The gatt_complete_event_t with type set to GATT_QUERY_COMPLETE, marks the end of read.
     */
    le_command_status_t gatt_client_read_characteristic_descriptor(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_descriptor_t  * descriptor);
    
    /** 
     * @brief Reads the long characteristic descriptor using its handle. It will be returned in several blobs. For each blob, an le_characteristic_descriptor_event_t with type set to GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT will be generated and passed to the registered callback. The gatt_complete_event_t with type set to GATT_QUERY_COMPLETE, marks the end of read.
     */
    le_command_status_t gatt_client_read_long_characteristic_descriptor(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_descriptor_t  * descriptor);
    
    /** 
     * @brief Writes the characteristic descriptor using its handle. The gatt_complete_event_t with type set to GATT_QUERY_COMPLETE, marks the end of write. The write is successfully performed, if the event's status field is set to 0.
     */
    le_command_status_t gatt_client_write_characteristic_descriptor(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_descriptor_t  * descriptor, uint16_t length, uint8_t  * data);
    le_command_status_t gatt_client_write_long_characteristic_descriptor(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_descriptor_t  * descriptor, uint16_t length, uint8_t  * data);
    
    /** 
     * @brief Writes the client characteristic configuration of the specified characteristic. It is used to subscribe for notifications or indications of the characteristic value. For notifications or indications specify: GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION resp. GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION as configuration value.
     */
    le_command_status_t gatt_client_write_client_characteristic_configuration(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_t * characteristic, uint16_t configuration);


## PAN API
<a name ="appendix:api_pan"></a>

    /** 
     * @brief Creates SDP record for PANU BNEP service in provided empty buffer.
     * @note Make sure the buffer is big enough.
     *
     * @param service is an empty buffer to store service record
     * @param security_desc 
     * @param name if NULL, the default service name will be assigned
     * @param description if NULL, the default service description will be assigned
     */
    void pan_create_panu_service(uint8_t *service, const char *name, const char *description, security_description_t security_desc);
    
    /** 
     * @brief Creates SDP record for GN BNEP service in provided empty buffer.
     * @note Make sure the buffer is big enough.
     *
     * @param service is an empty buffer to store service record
     * @param security_desc 
     * @param name if NULL, the default service name will be assigned
     * @param description if NULL, the default service description will be assigned
     * @param IPv4Subnet is optional subnet definition, e.g. "10.0.0.0/8"
     * @param IPv6Subnet is optional subnet definition given in the standard IETF format with the absolute attribute IDs
     */
    void pan_create_gn_service(uint8_t *service, const char *name, const char *description, security_description_t security_desc, 
    	const char *IPv4Subnet, const char *IPv6Subnet);
    
    /** 
     * @brief Creates SDP record for NAP BNEP service in provided empty buffer.
     * @note Make sure the buffer is big enough.
     *
     * @param service is an empty buffer to store service record
     * @param name if NULL, the default service name will be assigned
     * @param security_desc 
     * @param description if NULL, the default service description will be assigned
     * @param net_access_type type of available network access
     * @param max_net_access_rate based on net_access_type measured in byte/s
     * @param IPv4Subnet is optional subnet definition, e.g. "10.0.0.0/8"
     * @param IPv6Subnet is optional subnet definition given in the standard IETF format with the absolute attribute IDs
     */
    void pan_create_nap_service(uint8_t *service, const char *name, const char *description, security_description_t security_desc, 
    	net_access_type_t net_access_type, uint32_t max_net_access_rate, const char *IPv4Subnet, const char *IPv6Subnet);


## BNEP API
<a name ="appendix:api_bnep"></a>

    /**
     * @brief Set up BNEP.
     */
    void bnep_init(void);
    
    /**
     * @brief Check if a data packet can be send out.
     */
    int bnep_can_send_packet_now(uint16_t bnep_cid);
    
    /**
     * @brief Send a data packet.
     */
    int bnep_send(uint16_t bnep_cid, uint8_t *packet, uint16_t len);
    
    /**
     * @brief Set the network protocol filter.
     */
    int bnep_set_net_type_filter(uint16_t bnep_cid, bnep_net_filter_t *filter, uint16_t len);
    
    /**
     * @brief Set the multicast address filter.
     */
    int bnep_set_multicast_filter(uint16_t bnep_cid, bnep_multi_filter_t *filter, uint16_t len);
    
    /**
     * @brief Set security level required for incoming connections, need to be called before registering services.
     */
    void bnep_set_required_security_level(gap_security_level_t security_level);
    
    /**
     * @brief Register packet handler. 
     */
    void bnep_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size));
    
    /**
     * @brief Creates BNEP connection (channel) to a given server on a remote device with baseband address. A new baseband connection will be initiated if necessary. 
     */
    int bnep_connect(void * connection, bd_addr_t addr, uint16_t l2cap_psm, uint16_t uuid_dest);
    
    /**
     * @brief Disconnects BNEP channel with given identifier. 
     */
    void bnep_disconnect(bd_addr_t addr);
    
    /**
     * @brief Registers BNEP service, set a maximum frame size and assigns a packet handler. On embedded systems, use NULL for connection parameter. 
     */
    void bnep_register_service(void * connection, uint16_t service_uuid, uint16_t max_frame_size);
    
    /**
     * @brief Unregister BNEP service.
     */
    void bnep_unregister_service(uint16_t service_uuid);


## GAP API
<a name ="appendix:api_gap"></a>

    /**
     * @brief Enable/disable bonding. Default is enabled.
     * @param enabled
     */
    void gap_set_bondable_mode(int enabled);
    
    /**
     * @brief Start dedicated bonding with device. Disconnect after bonding.
     * @param device
     * @param request MITM protection
     * @return error, if max num acl connections active
     * @result GAP_DEDICATED_BONDING_COMPLETE
     */
    int gap_dedicated_bonding(bd_addr_t device, int mitm_protection_required);
    
    gap_security_level_t gap_security_level_for_link_key_type(link_key_type_t link_key_type);
    gap_security_level_t gap_security_level(hci_con_handle_t con_handle);
    
    void gap_request_security_level(hci_con_handle_t con_handle, gap_security_level_t level);
    int  gap_mitm_protection_required_for_security_level(gap_security_level_t level);
    
    /** 
     * @brief Sets local name.
     * @note has to be done before stack starts up
     * @param name is not copied, make sure memory is accessible during stack startup
     */
    void gap_set_local_name(const char * local_name);


## SM API
<a name ="appendix:api_sm"></a>

    /**
     * @brief Security Manager event
     */
    typedef struct sm_event {
        uint8_t   type;                 ///< See <btstack/hci_cmds.h> SM_...
        uint8_t   addr_type;
        bd_addr_t address;
        uint32_t  passkey;              ///< only used for SM_PASSKEY_DISPLAY_NUMBER 
        uint16_t  le_device_db_index;   ///< only used for SM_IDENTITY_RESOLVING_..
        uint8_t   authorization_result; ///< only use for SM_AUTHORIZATION_RESULT
    } sm_event_t;
    
    /**
     * @brief Initializes the Security Manager, connects to L2CAP
     */
    void sm_init(void);
    
    /**
     * @brief Set secret ER key for key generation as described in Core V4.0, Vol 3, Part G, 5.2.2 
     * @param er
     */
    void sm_set_er(sm_key_t er);
    
    /**
     * @brief Set secret IR key for key generation as described in Core V4.0, Vol 3, Part G, 5.2.2 
     */
    void sm_set_ir(sm_key_t ir);
    
    /**
     *
     * @brief Registers OOB Data Callback. The callback should set the oob_data and return 1 if OOB data is availble
     * @param get_oob_data_callback
     */
    void sm_register_oob_data_callback( int (*get_oob_data_callback)(uint8_t addres_type, bd_addr_t addr, uint8_t * oob_data));
    
    /**
     *
     * @brief Registers packet handler. Called by att_server.c
     */
    void sm_register_packet_handler(btstack_packet_handler_t handler);
    
    /**
     * @brief Limit the STK generation methods. Bonding is stopped if the resulting one isn't in the list
     * @param OR combination of SM_STK_GENERATION_METHOD_ 
     */
    void sm_set_accepted_stk_generation_methods(uint8_t accepted_stk_generation_methods);
    
    /**
     * @brief Set the accepted encryption key size range. Bonding is stopped if the result isn't within the range
     * @param min_size (default 7)
     * @param max_size (default 16)
     */
    void sm_set_encryption_key_size_range(uint8_t min_size, uint8_t max_size);
    
    /**
     * @brief Sets the requested authentication requirements, bonding yes/no, MITM yes/no
     * @param OR combination of SM_AUTHREQ_ flags
     */
    void sm_set_authentication_requirements(uint8_t auth_req);
    
    /**
     * @brief Sets the available IO Capabilities
     * @param IO_CAPABILITY_
     */
    void sm_set_io_capabilities(io_capability_t io_capability);
    
    /**
     * @brief Let Peripheral request an encrypted connection right after connecting
     * @note Not used normally. Bonding is triggered by access to protected attributes in ATT Server
     */
    void sm_set_request_security(int enable);
    
    /** 
     * @brief Trigger Security Request
     * @note Not used normally. Bonding is triggered by access to protected attributes in ATT Server
     */
    void sm_send_security_request(uint16_t handle);
    
    /**
     * @brief Decline bonding triggered by event before
     * @param addr_type and address
     */
    void sm_bonding_decline(uint8_t addr_type, bd_addr_t address);
    
    /**
     * @brief Confirm Just Works bonding 
     * @param addr_type and address
     */
    void sm_just_works_confirm(uint8_t addr_type, bd_addr_t address);
    
    /**
     * @brief Reports passkey input by user
     * @param addr_type and address
     * @param passkey in [0..999999]
     */
    void sm_passkey_input(uint8_t addr_type, bd_addr_t address, uint32_t passkey);
    
    /**
     *
     * @brief Get encryption key size.
     * @param addr_type and address
     * @return 0 if not encrypted, 7-16 otherwise
     */
    int sm_encryption_key_size(uint8_t addr_type, bd_addr_t address);
    
    /**
     * @brief Get authentication property.
     * @param addr_type and address
     * @return 1 if bonded with OOB/Passkey (AND MITM protection)
     */
    int sm_authenticated(uint8_t addr_type, bd_addr_t address);
    
    /**
     * @brief Queries authorization state.
     * @param addr_type and address
     * @return authorization_state for the current session
     */
    authorization_state_t sm_authorization_state(uint8_t addr_type, bd_addr_t address);
    
    /**
     * @brief Used by att_server.c to request user authorization.
     * @param addr_type and address
     */
    void sm_request_authorization(uint8_t addr_type, bd_addr_t address);
    
    /**
     * @brief Report user authorization decline.
     * @param addr_type and address
     */
    void sm_authorization_decline(uint8_t addr_type, bd_addr_t address);
    
    /**
     * @brief Report user authorization grant.
     * @param addr_type and address
     */
    void sm_authorization_grant(uint8_t addr_type, bd_addr_t address);
    
    /**
     * @brief Support for signed writes, used by att_server.
     * @note Message and result are in little endian to allows passing in ATT PDU without flipping them first.
     */
    int  sm_cmac_ready(void);
    void sm_cmac_start(sm_key_t k, uint16_t message_len, uint8_t * message, uint32_t sign_counter, void (*done_handler)(uint8_t hash[8]));
    
    /**
     * @brief Identify device in LE Device DB.
     * @param handle
     * @return index from le_device_db or -1 if not found/identified
     */
    int sm_le_device_index(uint16_t handle );
