#!/usr/bin/env python

import sys
import os
import subprocess

program_info = '''
Show BTstack RAM usage for compiled .elf file

Usage: {0} path/to/file.elf
'''

# Return a dict containing symbol_name: path/to/file/where/it/originates
# for all symbols from the .elf file. Optionnaly strips the path according
# to the passed sub-path
def load_symbols(elf_file, path_to_strip = None):
    symbols = []
    nm_out = subprocess.check_output(["arm-none-eabi-nm", elf_file, "-S", "-l", "--size-sort", "--radix=d"])
    for line in nm_out.split('\n'):
        fields = line.replace('\t', ' ').split(' ')
        # Get rid of trailing empty field
        if len(fields) == 1 and fields[0] == '':
            continue
        assert len(fields)>=4
        if len(fields)<5:
            path = ":/" + fields[3]
        else:
            path = fields[4].split(':')[0]
        if path_to_strip != None:
            if path_to_strip in path:
                path = path.replace(path_to_strip, "") + '/' + fields[3]
            else:
                path = ":/" + fields[3]
        dict = {}
        dict['offset'] = fields[0]
        dict['size']   = fields[1]
        dict['type']   = fields[2]
        dict['name']   = fields[3]
        if (len(fields) > 4):
            dict['path']   = fields[4]
        # dict['path']   = path
        symbols.append(dict)
    return symbols

if len(sys.argv) < 2:
    print (program_info.format(sys.argv[0]))
    sys.exit(0)


btstack_symbols = [
'att_client_handler', 
'att_client_packet_handler', 
'att_client_waiting_for_can_send', 
'att_client_waiting_for_can_send', 
'att_db', 
'att_prepare_write_error_code', 
'att_prepare_write_error_handle', 
'att_read_callback', 
'att_server_handler', 
'att_server_waiting_for_can_send', 
'att_write_callback', 
'battery', 
'battery_callback', 
'battery_service', 
'battery_value', 
'battery_value_client_configuration', 
'battery_value_client_configuration_connection', 
'battery_value_handle_client_configuration', 
'battery_value_handle_value', 
'bd_addr_to_str_buffer', 
'btstack_run_loop_rtc0_overflow_counter', 
'can_send_now_clients', 
'con_handle', 
'counter', 
'counter_string', 
'counter_string_len', 
'dkg_state', 
'dump_file', 
'fixed_channels', 
'gap_random_address_update_timer', 
'gap_random_adress_type', 
'gap_random_adress_update_period', 
'hci_connection_pool', 
'hci_connection_storage', 
'hci_event_callback_registration', 
'hci_rx_buffer', 
'hci_rx_pos', 
'hci_rx_type', 
'hci_stack', 
'hci_stack_static', 
'heartbeat', 
'l2cap_event_packet_handler', 
'le_devices', 
'le_notification_enabled', 
'rau_state', 
'service_handlers', 
'service_record_item_pool', 
'service_record_item_storage', 
'signaling_responses', 
'signaling_responses_pending', 
'sm_accepted_stk_generation_methods', 
'sm_active_connection', 
'sm_address_resolution_addr_type', 
'sm_address_resolution_address', 
'sm_address_resolution_ah_calculation_active', 
'sm_address_resolution_context', 
'sm_address_resolution_general_queue', 
'sm_address_resolution_mode', 
'sm_address_resolution_test', 
'sm_aes128_context', 
'sm_aes128_state', 
'sm_cmac_block_count', 
'sm_cmac_block_current', 
'sm_cmac_done_handler', 
'sm_cmac_get_byte', 
'sm_cmac_header', 
'sm_cmac_k', 
'sm_cmac_m_last', 
'sm_cmac_message', 
'sm_cmac_message_len', 
'sm_cmac_sign_counter', 
'sm_cmac_state', 
'sm_cmac_x', 
'sm_event_callback_registration', 
'sm_event_handlers', 
'sm_lookup_entry_pool', 
'sm_lookup_entry_storage', 
'sm_max_encryption_key_size', 
'sm_min_encryption_key_size', 
'sm_persistent_dhk', 
'sm_persistent_er', 
'sm_persistent_ir', 
'sm_persistent_irk', 
'sm_random_address', 
'sm_random_context', 
'test_use_fixed_local_csrk', 
'the_run_loop', 
'the_setup', 
'timers', 
'transport_packet_handler', 
'whitelist_entry_pool', 
'whitelist_entry_storage', 
]

symbols = load_symbols(sys.argv[1])
ram = 0
ram_symbols = 0
for dict in symbols:
    if dict['type'] in "tT":
      continue
    if 'path' in dict:
        path = dict['path']
    size = int(dict['size'])
    full_name = dict['name']
    name = full_name.split('.')[0]
    if not name in btstack_symbols:
        continue
    path = ''
    # print (name)
    print ("%-50s %s %5s %s" % (name, dict['type'], size, path))
    ram += size
    ram_symbols += 1
print ("BTstack Symbols %u" % ram_symbols)
print ("BTstack RAM:    %u" % ram)

