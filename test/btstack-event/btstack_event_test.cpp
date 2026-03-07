#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_defines.h"
#include "btstack_event.h"

TEST_GROUP(btstack_event){
    void setup(void){
    }
    void teardown(void){
    }
};

TEST(btstack_event, le_cs_read_remote_supported_capabilities_complete_getters){
    const uint8_t event[] = {
        HCI_EVENT_LE_META, 0x20, HCI_SUBEVENT_LE_CS_READ_REMOTE_SUPPORTED_CAPABILITIES_COMPLETE,
        0x01,
        0x34, 0x12,
        0x56,
        0x9a, 0x78,
        0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
        0x14, 0x13,
        0x16, 0x15,
        0x17,
        0x19, 0x18,
        0x1b, 0x1a,
        0x1d, 0x1c,
        0x1f, 0x1e,
        0x21, 0x20,
        0x22,
        0x23
    };

    CHECK_EQUAL(HCI_SUBEVENT_LE_CS_READ_REMOTE_SUPPORTED_CAPABILITIES_COMPLETE, hci_event_le_meta_get_subevent_code(event));
    CHECK_EQUAL(0x01, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_status(event));
    CHECK_EQUAL(0x1234, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_connection_handle(event));
    CHECK_EQUAL(0x56, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_num_config_supported(event));
    CHECK_EQUAL(0x789a, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_max_consecutive_procedures_supported(event));
    CHECK_EQUAL(0x0b, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_num_antennae_supported(event));
    CHECK_EQUAL(0x0c, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_max_antenna_paths_supported(event));
    CHECK_EQUAL(0x0d, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_roles_supported(event));
    CHECK_EQUAL(0x0e, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_modes_supported(event));
    CHECK_EQUAL(0x0f, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_rtt_capability(event));
    CHECK_EQUAL(0x10, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_rtt_aa_only_n(event));
    CHECK_EQUAL(0x11, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_rtt_sounding_n(event));
    CHECK_EQUAL(0x12, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_rtt_random_sequence_n(event));
    CHECK_EQUAL(0x1314, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_nadm_sounding_capability(event));
    CHECK_EQUAL(0x1516, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_nadm_random_capability(event));
    CHECK_EQUAL(0x17, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_cs_sync_phys_supported(event));
    CHECK_EQUAL(0x1819, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_subfeatures_supported(event));
    CHECK_EQUAL(0x1a1b, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_t_ip1_times_supported(event));
    CHECK_EQUAL(0x1c1d, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_t_ip2_times_supported(event));
    CHECK_EQUAL(0x1e1f, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_t_fcs_times_supported(event));
    CHECK_EQUAL(0x2021, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_t_pm_times_supported(event));
    CHECK_EQUAL(0x22, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_t_sw_time_supported(event));
    CHECK_EQUAL(0x23, hci_subevent_le_cs_read_remote_supported_capabilities_complete_get_tx_snr_capability(event));
}

TEST(btstack_event, le_cs_read_remote_fae_table_complete_getters){
    const uint8_t table_0[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };
    const uint8_t table_1[] = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
    };
    const uint8_t table_2[] = {
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f
    };
    const uint8_t table_3[] = {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
    };
    const uint8_t table_4[] = {
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47
    };
    const uint8_t event[] = {
        HCI_EVENT_LE_META, 0x4c, HCI_SUBEVENT_LE_CS_READ_REMOTE_FAE_TABLE_COMPLETE,
        0x7e,
        0x22, 0x11,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47
    };

    CHECK_EQUAL(HCI_SUBEVENT_LE_CS_READ_REMOTE_FAE_TABLE_COMPLETE, hci_event_le_meta_get_subevent_code(event));
    CHECK_EQUAL(0x7e, hci_subevent_le_cs_read_remote_fae_table_complete_get_status(event));
    CHECK_EQUAL(0x1122, hci_subevent_le_cs_read_remote_fae_table_complete_get_connection_handle(event));
    MEMCMP_EQUAL(table_0, hci_subevent_le_cs_read_remote_fae_table_complete_get_remote_fae_table_0(event), sizeof(table_0));
    MEMCMP_EQUAL(table_1, hci_subevent_le_cs_read_remote_fae_table_complete_get_remote_fae_table_1(event), sizeof(table_1));
    MEMCMP_EQUAL(table_2, hci_subevent_le_cs_read_remote_fae_table_complete_get_remote_fae_table_2(event), sizeof(table_2));
    MEMCMP_EQUAL(table_3, hci_subevent_le_cs_read_remote_fae_table_complete_get_remote_fae_table_3(event), sizeof(table_3));
    MEMCMP_EQUAL(table_4, hci_subevent_le_cs_read_remote_fae_table_complete_get_remote_fae_table_4(event), sizeof(table_4));
}

TEST(btstack_event, le_cs_security_enable_complete_getters){
    const uint8_t event[] = {
        HCI_EVENT_LE_META, 0x04, HCI_SUBEVENT_LE_CS_SECURITY_ENABLE_COMPLETE,
        0xab,
        0xfe, 0xca
    };

    CHECK_EQUAL(HCI_SUBEVENT_LE_CS_SECURITY_ENABLE_COMPLETE, hci_event_le_meta_get_subevent_code(event));
    CHECK_EQUAL(0xab, hci_subevent_le_cs_security_enable_complete_get_status(event));
    CHECK_EQUAL(0xcafe, hci_subevent_le_cs_security_enable_complete_get_connection_handle(event));
}

TEST(btstack_event, le_cs_config_complete_getters){
    const uint8_t event[] = {
        HCI_EVENT_LE_META, 0x22, HCI_SUBEVENT_LE_CS_CONFIG_COMPLETE,
        0x24,
        0x34, 0x12,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
        0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88,
        0x99, 0xaa,
        0xbb, 0xcc, 0xdd, 0xee, 0xff,
        0x12, 0x13, 0x14, 0x15
    };

    CHECK_EQUAL(HCI_SUBEVENT_LE_CS_CONFIG_COMPLETE, hci_event_le_meta_get_subevent_code(event));
    CHECK_EQUAL(0x24, hci_subevent_le_cs_config_complete_get_status(event));
    CHECK_EQUAL(0x1234, hci_subevent_le_cs_config_complete_get_connection_handle(event));
    CHECK_EQUAL(0x01, hci_subevent_le_cs_config_complete_get_config_id(event));
    CHECK_EQUAL(0x02, hci_subevent_le_cs_config_complete_get_action(event));
    CHECK_EQUAL(0x03, hci_subevent_le_cs_config_complete_get_main_mode_type(event));
    CHECK_EQUAL(0x04, hci_subevent_le_cs_config_complete_get_sub_mode_type(event));
    CHECK_EQUAL(0x05, hci_subevent_le_cs_config_complete_get_min_main_mode_steps(event));
    CHECK_EQUAL(0x06, hci_subevent_le_cs_config_complete_get_max_main_mode_steps(event));
    CHECK_EQUAL(0x07, hci_subevent_le_cs_config_complete_get_main_mode_repetition(event));
    CHECK_EQUAL(0x08, hci_subevent_le_cs_config_complete_get_mode_0_steps(event));
    CHECK_EQUAL(0x09, hci_subevent_le_cs_config_complete_get_role(event));
    CHECK_EQUAL(0x0a, hci_subevent_le_cs_config_complete_get_rtt_type(event));
    CHECK_EQUAL(0x0b, hci_subevent_le_cs_config_complete_get_cs_sync_phy(event));
    CHECK_EQUAL(0x44332211u, hci_subevent_le_cs_config_complete_get_channel_map_0_3(event));
    CHECK_EQUAL(0x88776655u, hci_subevent_le_cs_config_complete_get_channel_map_4_7(event));
    CHECK_EQUAL(0xaa99, hci_subevent_le_cs_config_complete_get_channel_map_8_9(event));
    CHECK_EQUAL(0xbb, hci_subevent_le_cs_config_complete_get_channel_map_repetition(event));
    CHECK_EQUAL(0xcc, hci_subevent_le_cs_config_complete_get_channel_selection_type(event));
    CHECK_EQUAL(0xdd, hci_subevent_le_cs_config_complete_get_ch3c_shape(event));
    CHECK_EQUAL(0xee, hci_subevent_le_cs_config_complete_get_ch3c_jump(event));
    CHECK_EQUAL(0xff, hci_subevent_le_cs_config_complete_get_reserved(event));
    CHECK_EQUAL(0x12, hci_subevent_le_cs_config_complete_get_t_ip1_time(event));
    CHECK_EQUAL(0x13, hci_subevent_le_cs_config_complete_get_t_ip2_time(event));
    CHECK_EQUAL(0x14, hci_subevent_le_cs_config_complete_get_t_fcs_time(event));
    CHECK_EQUAL(0x15, hci_subevent_le_cs_config_complete_get_t_pm_time(event));
}

TEST(btstack_event, le_cs_procedure_enable_complete_getters){
    const uint8_t event[] = {
        HCI_EVENT_LE_META, 0x16, HCI_SUBEVENT_LE_CS_PROCEDURE_ENABLE_COMPLETE,
        0x01,
        0x01, 0x22,
        0x02, 0x03, 0x04, 0x05,
        0x08, 0x07, 0x06,
        0x09,
        0x0a, 0x0b,
        0x0c, 0x0d,
        0x0e, 0x0f,
        0x10, 0x11,
        0x12, 0x13
    };

    CHECK_EQUAL(HCI_SUBEVENT_LE_CS_PROCEDURE_ENABLE_COMPLETE, hci_event_le_meta_get_subevent_code(event));
    CHECK_EQUAL(0x01, hci_subevent_le_cs_procedure_enable_complete_get_status(event));
    CHECK_EQUAL(0x2201, hci_subevent_le_cs_procedure_enable_complete_get_connection_handle(event));
    CHECK_EQUAL(0x02, hci_subevent_le_cs_procedure_enable_complete_get_config_id(event));
    CHECK_EQUAL(0x03, hci_subevent_le_cs_procedure_enable_complete_get_state(event));
    CHECK_EQUAL(0x04, hci_subevent_le_cs_procedure_enable_complete_get_tone_antenna_config_selection(event));
    CHECK_EQUAL(5, hci_subevent_le_cs_procedure_enable_complete_get_selected_tx_power(event));
    CHECK_EQUAL(0x060708u, hci_subevent_le_cs_procedure_enable_complete_get_subevent_len(event));
    CHECK_EQUAL(0x09, hci_subevent_le_cs_procedure_enable_complete_get_subevents_per_event(event));
    CHECK_EQUAL(0x0b0a, hci_subevent_le_cs_procedure_enable_complete_get_subevent_interval(event));
    CHECK_EQUAL(0x0d0c, hci_subevent_le_cs_procedure_enable_complete_get_event_interval(event));
    CHECK_EQUAL(0x0f0e, hci_subevent_le_cs_procedure_enable_complete_get_procedure_interval(event));
    CHECK_EQUAL(0x1110, hci_subevent_le_cs_procedure_enable_complete_get_procedure_count(event));
    CHECK_EQUAL(0x1312, hci_subevent_le_cs_procedure_enable_complete_get_max_procedure_len(event));
}

TEST(btstack_event, le_cs_subevent_result_getters){
    const uint8_t event[] = {
        HCI_EVENT_LE_META, 0x19, HCI_SUBEVENT_LE_CS_SUBEVENT_RESULT,
        0x78, 0x56,
        0x34,
        0x12, 0x11,
        0x14, 0x13,
        0x16, 0x15,
        0x17, 0x18, 0x19, 0x1a, 0x1b,
        0x02,
        0xa1, 0xb1, 0x02, 0xc1, 0xc2,
        0xa2, 0xb2, 0x01, 0xd1
    };

    CHECK_EQUAL(HCI_SUBEVENT_LE_CS_SUBEVENT_RESULT, hci_event_le_meta_get_subevent_code(event));
    CHECK_EQUAL(0x5678, hci_subevent_le_cs_subevent_result_get_connection_handle(event));
    CHECK_EQUAL(0x34, hci_subevent_le_cs_subevent_result_get_config_id(event));
    CHECK_EQUAL(0x1112, hci_subevent_le_cs_subevent_result_get_start_acl_conn_event_counter(event));
    CHECK_EQUAL(0x1314, hci_subevent_le_cs_subevent_result_get_procedure_counter(event));
    CHECK_EQUAL((int16_t) 0x1516, hci_subevent_le_cs_subevent_result_get_frequency_compensation(event));
    CHECK_EQUAL(23, hci_subevent_le_cs_subevent_result_get_reference_power_level(event));
    CHECK_EQUAL(0x18, hci_subevent_le_cs_subevent_result_get_procedure_done_status(event));
    CHECK_EQUAL(0x19, hci_subevent_le_cs_subevent_result_get_subevent_done_status(event));
    CHECK_EQUAL(0x1a, hci_subevent_le_cs_subevent_result_get_abort_reason(event));
    CHECK_EQUAL(0x1b, hci_subevent_le_cs_subevent_result_get_num_antenna_paths(event));
    CHECK_EQUAL(0x02, hci_subevent_le_cs_subevent_result_get_num_steps_reported(event));

    btstack_event_iterator_t iter;
    hci_subevent_le_cs_subevent_result_steps_init(&iter, event);
    CHECK(hci_subevent_le_cs_subevent_result_steps_has_next(&iter));
    CHECK_EQUAL(0xa1, hci_subevent_le_cs_subevent_result_get_steps_item_step_mode(&iter));
    CHECK_EQUAL(0xb1, hci_subevent_le_cs_subevent_result_get_steps_item_step_channel(&iter));
    CHECK_EQUAL(0x02, hci_subevent_le_cs_subevent_result_get_steps_item_step_data_length(&iter));
    const uint8_t * step_data = hci_subevent_le_cs_subevent_result_get_steps_item_step_data(&iter);
    const uint8_t expected_step_data_1[] = { 0xc1, 0xc2 };
    MEMCMP_EQUAL(expected_step_data_1, step_data, sizeof(expected_step_data_1));

    hci_subevent_le_cs_subevent_result_steps_next(&iter);
    CHECK(hci_subevent_le_cs_subevent_result_steps_has_next(&iter));
    CHECK_EQUAL(0xa2, hci_subevent_le_cs_subevent_result_get_steps_item_step_mode(&iter));
    CHECK_EQUAL(0xb2, hci_subevent_le_cs_subevent_result_get_steps_item_step_channel(&iter));
    CHECK_EQUAL(0x01, hci_subevent_le_cs_subevent_result_get_steps_item_step_data_length(&iter));
    step_data = hci_subevent_le_cs_subevent_result_get_steps_item_step_data(&iter);
    CHECK_EQUAL(0xd1, step_data[0]);

    hci_subevent_le_cs_subevent_result_steps_next(&iter);
    CHECK_FALSE(hci_subevent_le_cs_subevent_result_steps_has_next(&iter));
}

TEST(btstack_event, cs_subevent_result_iterator){
    uint8_t event[64];
    const uint16_t connection_handle = 0x0001;
    const uint8_t config_id = 0x02;

    // build HCI LE Meta event with CS Subevent Result
    event[0] = HCI_EVENT_LE_META;
    event[2] = HCI_SUBEVENT_LE_CS_SUBEVENT_RESULT;

    uint8_t pos = 3;
    event[pos++] = connection_handle & 0xFFu;
    event[pos++] = connection_handle >> 8;
    event[pos++] = config_id;
    // start_acl_conn_event_counter
    event[pos++] = 0x10; event[pos++] = 0x00;
    // procedure_counter
    event[pos++] = 0x20; event[pos++] = 0x00;
    // frequency_compensation
    event[pos++] = 0x30; event[pos++] = 0x00;
    // reference_power_level
    event[pos++] = 0x40;
    // procedure_done_status
    event[pos++] = 0x01;
    // subevent_done_status
    event[pos++] = 0x02;
    // abort_reason
    event[pos++] = 0x03;
    // num_antenna_paths
    event[pos++] = 0x04;
    // num_steps_reported
    event[pos++] = 0x02;

    // step 0
    event[pos++] = 0x11; // step_mode
    event[pos++] = 0x22; // step_channel
    event[pos++] = 0x03; // step_data_length
    event[pos++] = 0xA1;
    event[pos++] = 0xA2;
    event[pos++] = 0xA3;

    // step 1
    event[pos++] = 0x33; // step_mode
    event[pos++] = 0x44; // step_channel
    event[pos++] = 0x01; // step_data_length
    event[pos++] = 0xB1;

    // payload length
    event[1] = (uint8_t)(pos - 2);

    btstack_event_iterator_t iter;
    hci_subevent_le_cs_subevent_result_steps_init(&iter, event);

    CHECK_TRUE(hci_subevent_le_cs_subevent_result_steps_has_next(&iter));
    CHECK_EQUAL(0x11, hci_subevent_le_cs_subevent_result_get_steps_item_step_mode(&iter));
    CHECK_EQUAL(0x22, hci_subevent_le_cs_subevent_result_get_steps_item_step_channel(&iter));
    CHECK_EQUAL(0x03, hci_subevent_le_cs_subevent_result_get_steps_item_step_data_length(&iter));
    const uint8_t *step_data_0 = hci_subevent_le_cs_subevent_result_get_steps_item_step_data(&iter);
    const uint8_t expected_step_0[] = { 0xA1, 0xA2, 0xA3 };
    MEMCMP_EQUAL(expected_step_0, step_data_0, sizeof(expected_step_0));
    hci_subevent_le_cs_subevent_result_steps_next(&iter);

    CHECK_TRUE(hci_subevent_le_cs_subevent_result_steps_has_next(&iter));
    CHECK_EQUAL(0x33, hci_subevent_le_cs_subevent_result_get_steps_item_step_mode(&iter));
    CHECK_EQUAL(0x44, hci_subevent_le_cs_subevent_result_get_steps_item_step_channel(&iter));
    CHECK_EQUAL(0x01, hci_subevent_le_cs_subevent_result_get_steps_item_step_data_length(&iter));
    const uint8_t *step_data_1 = hci_subevent_le_cs_subevent_result_get_steps_item_step_data(&iter);
    const uint8_t expected_step_1[] = { 0xB1 };
    MEMCMP_EQUAL(expected_step_1, step_data_1, sizeof(expected_step_1));
    hci_subevent_le_cs_subevent_result_steps_next(&iter);

    CHECK_FALSE(hci_subevent_le_cs_subevent_result_steps_has_next(&iter));
}

TEST(btstack_event, signed_event_getters){
    const uint8_t inquiry_result_with_rssi[] = {
        HCI_EVENT_INQUIRY_RESULT_WITH_RSSI, 0x0e,
        0x01,
        0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x02,
        0x00,
        0xaa, 0xbb, 0xcc,
        0x34, 0x12,
        0xfb
    };
    CHECK_EQUAL(-5, hci_event_inquiry_result_with_rssi_get_rssi(inquiry_result_with_rssi));

    const uint8_t gap_adv_report[] = {
        GAP_EVENT_ADVERTISING_REPORT, 0x0b,
        0x00,
        0x01,
        0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0xf6,
        0x01,
        0xaa
    };
    CHECK_EQUAL(-10, gap_event_advertising_report_get_rssi(gap_adv_report));

    const uint8_t gap_ext_adv_report[] = {
        GAP_EVENT_EXTENDED_ADVERTISING_REPORT, 0x19,
        0x34, 0x12,
        0x01,
        0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x02,
        0x03,
        0x04,
        0xf7,
        0xec,
        0x20, 0x00,
        0x00,
        0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x07,
        0x01,
        0xbb
    };
    CHECK_EQUAL(-9, gap_event_extended_advertising_report_get_tx_power(gap_ext_adv_report));
    CHECK_EQUAL(-20, gap_event_extended_advertising_report_get_rssi(gap_ext_adv_report));

    const uint8_t le_periodic_advertising_report[] = {
        HCI_EVENT_LE_META, 0x09, HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_REPORT,
        0x34, 0x12,
        0xf7,
        0xec,
        0x01,
        0x00,
        0x01,
        0xaa
    };
    CHECK_EQUAL(-9, hci_subevent_le_periodic_advertising_report_get_tx_power(le_periodic_advertising_report));
    CHECK_EQUAL(-20, hci_subevent_le_periodic_advertising_report_get_rssi(le_periodic_advertising_report));

    const uint8_t le_transmit_power_reporting[] = {
        HCI_EVENT_LE_META, 0x0a, HCI_SUBEVENT_LE_TRANSMIT_POWER_REPORTING,
        0x00,
        0x34, 0x12,
        0x01,
        0x02,
        0xf8,
        0x03,
        0xfe
    };
    CHECK_EQUAL(-8, hci_subevent_le_transmit_power_reporting_get_tx_power_level(le_transmit_power_reporting));
    CHECK_EQUAL(-2, hci_subevent_le_transmit_power_reporting_get_delta(le_transmit_power_reporting));
}

TEST(btstack_event, cs_subevent_result_continue_iterator){
    uint8_t event[64];
    const uint16_t connection_handle = 0x0002;
    const uint8_t config_id = 0x01;

    // build HCI LE Meta event with CS Subevent Result Continue
    event[0] = HCI_EVENT_LE_META;
    event[2] = HCI_SUBEVENT_LE_CS_SUBEVENT_RESULT_CONTINUE;

    uint8_t pos = 3;
    event[pos++] = connection_handle & 0xFFu;
    event[pos++] = connection_handle >> 8;
    event[pos++] = config_id;
    // procedure_done_status
    event[pos++] = 0x0F;
    // subevent_done_status
    event[pos++] = 0x0E;
    // abort_reason
    event[pos++] = 0x0D;
    // num_antenna_paths
    event[pos++] = 0x01;
    // num_steps_reported
    event[pos++] = 0x02;

    // step 0
    event[pos++] = 0x55; // step_mode
    event[pos++] = 0x66; // step_channel
    event[pos++] = 0x02; // step_data_length
    event[pos++] = 0xC1;
    event[pos++] = 0xC2;

    // step 1
    event[pos++] = 0x77; // step_mode
    event[pos++] = 0x88; // step_channel
    event[pos++] = 0x03; // step_data_length
    event[pos++] = 0xD1;
    event[pos++] = 0xD2;
    event[pos++] = 0xD3;

    // payload length
    event[1] = (uint8_t)(pos - 2);

    btstack_event_iterator_t iter;
    hci_subevent_le_cs_subevent_result_continue_steps_init(&iter, event);

    CHECK_TRUE(hci_subevent_le_cs_subevent_result_continue_steps_has_next(&iter));
    CHECK_EQUAL(0x55, hci_subevent_le_cs_subevent_result_continue_get_steps_item_step_mode(&iter));
    CHECK_EQUAL(0x66, hci_subevent_le_cs_subevent_result_continue_get_steps_item_step_channel(&iter));
    CHECK_EQUAL(0x02, hci_subevent_le_cs_subevent_result_continue_get_steps_item_step_data_length(&iter));
    const uint8_t *step_data = hci_subevent_le_cs_subevent_result_continue_get_steps_item_step_data(&iter);
    const uint8_t expected_step[] = { 0xC1, 0xC2 };
    MEMCMP_EQUAL(expected_step, step_data, sizeof(expected_step));
    hci_subevent_le_cs_subevent_result_continue_steps_next(&iter);

    CHECK_TRUE(hci_subevent_le_cs_subevent_result_continue_steps_has_next(&iter));
    CHECK_EQUAL(0x77, hci_subevent_le_cs_subevent_result_continue_get_steps_item_step_mode(&iter));
    CHECK_EQUAL(0x88, hci_subevent_le_cs_subevent_result_continue_get_steps_item_step_channel(&iter));
    CHECK_EQUAL(0x03, hci_subevent_le_cs_subevent_result_continue_get_steps_item_step_data_length(&iter));
    const uint8_t *step_data_1 = hci_subevent_le_cs_subevent_result_continue_get_steps_item_step_data(&iter);
    const uint8_t expected_step_1[] = { 0xD1, 0xD2, 0xD3 };
    MEMCMP_EQUAL(expected_step_1, step_data_1, sizeof(expected_step_1));
    hci_subevent_le_cs_subevent_result_continue_steps_next(&iter);

    CHECK_FALSE(hci_subevent_le_cs_subevent_result_continue_steps_has_next(&iter));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
