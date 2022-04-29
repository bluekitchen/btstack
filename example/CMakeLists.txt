# list of examples sorted by Bluetooth Mode configurations

# List of General Examples without Bluetooth

set(EXAMPLES_GENERAL
	audio_duplex
	led_counter
	mod_player
	sine_player
)

# List of Examples that only use Bluetooth BR/EDR = Classic

set(EXAMPLES_CLASSIC_ONLY
	a2dp_sink_demo
	a2dp_source_demo
	avrcp_browsing_client
	dut_mode_classic
	gap_dedicated_bonding
	gap_inquiry
	gap_link_keys
	hfp_ag_demo
	hfp_hf_demo
	hid_host_demo
	hid_keyboard_demo
	hid_mouse_demo
	hsp_ag_demo
	hsp_hs_demo
	pbap_client_demo
	sdp_bnep_query
	sdp_general_query      
	sdp_rfcomm_query
	spp_counter
	spp_streamer
	spp_streamer_client    
	ublox_spp_le_counter   
)

# List of Examples that only use Bluetooth LE

set(EXAMPLES_LE_ONLY
	ancs_client_demo
	att_delayed_response   
	gap_le_advertisements  
	gatt_battery_query     
	gatt_browser
	gatt_counter
	gatt_device_information_query
	gatt_heart_rate_client 
	gatt_streamer_server   
	hog_boot_host_demo     
	hog_host_demo
	hog_keyboard_demo      
	hog_mouse_demo
	le_credit_based_flow_control_mode_client 
	le_credit_based_flow_control_mode_server 
	le_mitm
	le_streamer_client     
	mesh_node_demo
	nordic_spp_le_counter  
	nordic_spp_le_streamer 
	sm_pairing_central     
	sm_pairing_peripheral  
	ublox_spp_le_counter   
)

# List of Examples that use Bluetooth BR/EDR/LE = Dual Mode

set(EXAMPLES_DUAL_MODE
	gatt_counter
	gatt_streamer_server   
	spp_and_gatt_counter   
	spp_and_gatt_streamer  
)

# List of GATT files used by either LE_ONLY or DUAL_MODE examples

set(EXAMPLES_GATT_FILES
	ancs_client_demo.gatt      
	att_delayed_response.gatt  
	gatt_battery_query.gatt    
	gatt_browser.gatt
	gatt_counter.gatt
	gatt_device_information_query.gatt
	gatt_streamer_server.gatt  
	hog_keyboard_demo.gatt     
	hog_mouse_demo.gatt
	le_credit_based_flow_control_mode_server.gatt
	nordic_spp_le_counter.gatt 
	nordic_spp_le_streamer.gatt
	sm_pairing_central.gatt    
	sm_pairing_peripheral.gatt 
	spp_and_gatt_counter.gatt  
	spp_and_gatt_streamer.gatt 
	ublox_spp_le_counter.gatt 
)
