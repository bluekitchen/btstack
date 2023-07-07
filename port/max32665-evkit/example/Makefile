
SUBDIRS =  \
hog_mouse_demo \
sdp_general_query \
dut_mode_classic \
le_credit_based_flow_control_mode_server \
gatt_counter \
spp_streamer_client \
audio_duplex \
pbap_client_demo \
hid_keyboard_demo \
spp_flowcontrol \
hog_host_demo \
pan_lwip_http_server \
avrcp_browsing_client \
hid_host_demo \
hog_boot_host_demo \
nordic_spp_le_streamer \
spp_streamer \
gatt_device_information_query \
hid_mouse_demo \
sine_player \
gap_dedicated_bonding \
gatt_heart_rate_client \
mesh_node_demo \
att_delayed_response \
hog_keyboard_demo \
spp_and_gatt_counter \
gap_le_advertisements \
gatt_battery_query \
hfp_ag_demo \
sdp_bnep_query \
sdp_rfcomm_query \
gap_link_keys \
hsp_ag_demo \
mod_player \
hsp_hs_demo \
nordic_spp_le_counter \
le_streamer_client \
ublox_spp_le_counter \
sm_pairing_peripheral \
spp_counter \
gatt_streamer_server \
spp_and_gatt_streamer \
sm_pairing_central \
le_mitm \
gatt_browser \
a2dp_source_demo \
hfp_hf_demo \
gap_inquiry \
a2dp_sink_demo \
ancs_client_demo \
led_counter \
le_credit_based_flow_control_mode_client

all:
	echo Building all examples
	for dir in $(SUBDIRS); do \
	$(MAKE) -C $$dir || exit 1; \
	done

clean:
	echo Cleaning all ports
	for dir in $(SUBDIRS); do \
	$(MAKE) -C $$dir clean; \
	done
