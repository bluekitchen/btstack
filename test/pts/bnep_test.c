/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "bnep_test.c"

/*
 * bnep_test.c : Tool for testig BNEP with PTS
 * based on panu_demo implemented by Ole Reinhardt <ole.reinhardt@kernelconcepts.de>
 */

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "btstack_stdin.h"
#include "classic/pan.h"
#include "classic/sdp_server.h"
#include "classic/sdp_util.h"

#define HARDWARE_TYPE_ETHERNET 0x0001

#define NETWORK_TYPE_IPv4       0x0800
#define NETWORK_TYPE_ARP        0x0806
#define NETWORK_TYPE_IPv6       0x86DD

#define IP_PROTOCOL_ICMP_IPv4   0x0001
#define IP_PROTOCOL_ICMP_IPv6   0x003a
#define IP_PROTOCOL_UDP         0x0011
#define IPv4_

#define ICMP_V4_TYPE_PING_REQUEST  0x08
#define ICMP_V4_TYPE_PING_RESPONSE 0x00

#define ICMP_V6_TYPE_PING_REQUEST  0x80
#define ICMP_V6_TYPE_PING_RESPONSE 0x81

#define ICMP_V6_TYPE_NEIGHBOR_SOLICITATION  0x87
#define ICMP_V6_TYPE_NEIGHBOR_ADVERTISEMENT 0x88

#define ARP_OPERATION_REQUEST 1
#define ARP_OPERATION_REPLY   2

// prototypes
static void show_usage(void);
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// Configuration for PTS
static bd_addr_t pts_addr = {0x00,0x1b,0xDC,0x07,0x32,0xEF};
//static bd_addr_t pts_addr = {0xE0,0x06,0xE6,0xBB,0x95,0x79}; // Ole Thinkpad
// static bd_addr_t other_addr = { 0x33, 0x33, 0x00, 0x00, 0x00, 0x16};
static bd_addr_t other_addr = { 0,0,0,0,0,0};

// broadcast
static bd_addr_t broadcast_addr = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
// static bd_addr_t broadcast_addr = { 0x33, 0x33, 0x00, 0x01, 0x00, 0x03 };

// Outgoing: Must match PTS TSPX_UUID_src_addresss 
static uint16_t bnep_src_uuid     = 0x1115; 

// Outgoing: Must match PTS TSPX_UUID_dest_address
static uint32_t bnep_dest_uuid    = 0x1116;

// Incoming: Must macht PTS TSPX_UUID_dest_address
static uint16_t bnep_local_service_uuid = 0x1116;


// Sample network protocol type filter set:
//   Ethernet type/length values the range 0x0000 - 0x05dc (Length), 0x05dd - 0x05ff (Reserved in IEEE 802.3)
//   Ethernet type 0x0600-0xFFFF
static bnep_net_filter_t network_protocol_filter [3] = {{0x0000, 0x05dc}, {0x05dd, 0x05ff}, {0x0600, 0xFFFF}};

// Sample multicast filter set:
//   Multicast filter range set to 00:00:00:00:00:00 - 00:00:00:00:00:00 means: We do not want to receive any multicast traffic
//   Ethernet type 0x0600-0xFFFF
static bnep_multi_filter_t multicast_filter [1] = {{{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}};



// state
static bd_addr_t local_addr;
static uint16_t bnep_l2cap_psm      = 0x000f;
static uint16_t bnep_cid            = 0;

static uint8_t network_buffer[BNEP_MTU_MIN];
static size_t  network_buffer_len = 0;

static uint8_t panu_sdp_record[200];

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void hexdumpf(const void *data, int size){
    char buffer[6*16+1];
    int i, j;

    uint8_t low = 0x0F;
    uint8_t high = 0xF0;
    j = 0;
    for (i=0; i<size;i++){
        uint8_t byte = ((uint8_t *)data)[i];
        buffer[j++] = '0';
        buffer[j++] = 'x';
        buffer[j++] = char_for_nibble((byte & high) >> 4);
        buffer[j++] = char_for_nibble(byte & low);
        buffer[j++] = ',';
        buffer[j++] = ' ';     
        if (j >= 6*16 ){
            buffer[j] = 0;
            printf("%s\n", buffer);
            j = 0;
        }
    }
    if (j != 0){
        buffer[j] = 0;
        printf("%s\n", buffer);
    }
}

static uint16_t setup_ethernet_header(int src_compressed, int dst_compressed, int broadcast, uint16_t network_protocol_type){
    // setup packet
    int pos = 0;
    // destination
    if (broadcast){
        bd_addr_copy(&network_buffer[pos], broadcast_addr);
    } else {
        bd_addr_copy(&network_buffer[pos], dst_compressed ? pts_addr : other_addr);
    }
    pos += 6;
    // source
    bd_addr_copy(&network_buffer[pos], src_compressed ? local_addr   : other_addr);
    pos += 6;
    big_endian_store_16(network_buffer, pos, network_protocol_type);
    pos += 2;
    return pos;
}

static void send_buffer(uint16_t pos){
    network_buffer_len = pos;
    if (bnep_can_send_packet_now(bnep_cid)) {
        bnep_send(bnep_cid, network_buffer, network_buffer_len);
        network_buffer_len = 0;
    }
}

static void send_ethernet_packet(int src_compressed, int dst_compressed){
    int pos = setup_ethernet_header(src_compressed, dst_compressed, 0, NETWORK_TYPE_IPv4); // IPv4
    // dummy data Ethernet packet
    int i;
    for (i = 60; i >= 0 ; i--){
        network_buffer[pos++] = i;
    }
    // test data payload
    for (i = 0; i < 0x5a0 ; i++){
        network_buffer[pos++] = i;
    }
    send_buffer(pos);
}

static void set_network_protocol_filter(void){
    bnep_set_net_type_filter(bnep_cid, network_protocol_filter, 3);
}

static void set_multicast_filter(void){
    bnep_set_multicast_filter(bnep_cid, multicast_filter, 1);
}

/* From RFC 5227 - 2.1.1
   A host probes to see if an address is already in use by broadcasting
   an ARP Request for the desired address.  The client MUST fill in the
   'sender hardware address' field of the ARP Request with the hardware
   address of the interface through which it is sending the packet.  The
   'sender IP address' field MUST be set to all zeroes; this is to avoid
   polluting ARP caches in other hosts on the same link in the case
   where the address turns out to be already in use by another host.
   The 'target hardware address' field is ignored and SHOULD be set to
   all zeroes.  The 'target IP address' field MUST be set to the address
   being probed.  An ARP Request constructed this way, with an all-zero
   'sender IP address', is referred to as an 'ARP Probe'.
*/

static void send_arp_probe_ipv4(void){

    // "random address"
    static uint8_t requested_address[4] = {169, 254, 1, 0};
    requested_address[3]++;

    int pos = setup_ethernet_header(1, 0, 1, NETWORK_TYPE_IPv4); 
    big_endian_store_16(network_buffer, pos, HARDWARE_TYPE_ETHERNET);
    pos += 2;
    big_endian_store_16(network_buffer, pos, NETWORK_TYPE_IPv4);
    pos += 2;
    network_buffer[pos++] = 6; // Hardware length (HLEN) - 6 MAC  Address
    network_buffer[pos++] = 4; // Protocol length (PLEN) - 4 IPv4 Address
    big_endian_store_16(network_buffer, pos, ARP_OPERATION_REQUEST); 
    pos += 2;
    bd_addr_copy(&network_buffer[pos], local_addr); // Sender Hardware Address (SHA)
    pos += 6;
    memset(&network_buffer[pos], 0, 4);                 // Sender Protocol Adress (SPA)
    pos += 4;
    bd_addr_copy(&network_buffer[pos], other_addr); // Target Hardware Address (THA) (ignored for requests)
    pos += 6;
    memcpy(&network_buffer[pos], requested_address, 4);
    pos += 4;
    // magically, add some extra bytes for Ethernet padding
    pos += 18;
    send_buffer(pos);
}

static uint16_t sum_ones_complement(uint16_t a, uint16_t b){
    uint32_t sum = a + b;
    while (sum > 0xffff){
        sum = (sum & 0xffff) + 1;
    }
    return sum;
}

static uint16_t calc_internet_checksum(uint8_t * data, int size){
    uint32_t checksum = 0;
    while (size){
        // add 16-bit value
        checksum = sum_ones_complement(checksum, big_endian_read_16(data, 0));
        data += 2;
        size -= 2;
    }
    return checksum;
}

static void send_ping_request_ipv4(void){

    uint8_t ipv4_header[] = {
        // ip
        0x45, 0x00, 0x00, 0x00,   // version + ihl, dscp } ecn, total len
        0x00, 0x00, 0x00, 0x00, // identification (16), flags + fragment offset
        0x01, 0x01, 0x00, 0x00, // time to live, procotol: icmp, checksum (16),
        0x00, 0x00, 0x00, 0x00, // source IP address
        0x00, 0x00, 0x00, 0x00, // destination IP address
    };

    uint8_t icmp_packet[] = {
        // icmp
        0x08, 0x00, 0x00, 0x00, // type: 0x08 PING Request
        0x00, 0x00, 0x00, 0x00
    };

    // ethernet header
    int pos = setup_ethernet_header(1, 0, 0, NETWORK_TYPE_IPv4); // IPv4
    
    // ipv4
    int total_length = sizeof(ipv4_header) + sizeof(icmp_packet);
    big_endian_store_16(ipv4_header, 2, total_length);
    uint16_t ipv4_checksum = calc_internet_checksum(ipv4_header, sizeof(ipv4_header));
    big_endian_store_16(ipv4_header, 10, ipv4_checksum);    
    // TODO: also set src/dest ip address
    memcpy(&network_buffer[pos], ipv4_header, sizeof(ipv4_header));
    pos += sizeof(ipv4_header);

    // icmp
    uint16_t icmp_checksum = calc_internet_checksum(icmp_packet, sizeof(icmp_packet));
    big_endian_store_16(icmp_packet, 2, icmp_checksum);    
    memcpy(&network_buffer[pos], icmp_packet, sizeof(icmp_packet));
    pos += sizeof(icmp_packet);

    // send
    send_buffer(pos);
}

static void send_ping_response_ipv4(void){

    uint8_t ipv4_header[] = {
        // ip
        0x45, 0x00, 0x00, 0x00,   // version + ihl, dscp } ecn, total len
        0x00, 0x00, 0x00, 0x00, // identification (16), flags + fragment offset
        0x01, 0x01, 0x00, 0x00, // time to live, procotol: icmp, checksum (16),
        0x00, 0x00, 0x00, 0x00, // source IP address
        0x00, 0x00, 0x00, 0x00, // destination IP address
    };

    uint8_t icmp_packet[] = {
        // icmp
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    // ethernet header
    int pos = setup_ethernet_header(1, 0, 0, NETWORK_TYPE_IPv4); // IPv4
    
    // ipv4
    int total_length = sizeof(ipv4_header) + sizeof(icmp_packet);
    big_endian_store_16(ipv4_header, 2, total_length);
    uint16_t ipv4_checksum = calc_internet_checksum(ipv4_header, sizeof(ipv4_header));
    big_endian_store_16(ipv4_header, 10, ipv4_checksum);    
    // TODO: also set src/dest ip address
    memcpy(&network_buffer[pos], ipv4_header, sizeof(ipv4_header));
    pos += sizeof(ipv4_header);

    // icmp
    uint16_t icmp_checksum = calc_internet_checksum(icmp_packet, sizeof(icmp_packet));
    big_endian_store_16(icmp_packet, 2, icmp_checksum);    
    memcpy(&network_buffer[pos], icmp_packet, sizeof(icmp_packet));
    pos += sizeof(icmp_packet);

    // send
    send_buffer(pos);
}

/* Untested */
static void send_ping_request_ipv6(void){

    uint8_t ipv6_header[] = {
        // ip
        0x60, 0x00, 0x00, 0x00, // version (4) + traffic class (8) + flow label (24)
        0x00, 0x00,   58, 0x01, // payload length(16), next header = IPv6-ICMP, hop limit
        0x00, 0x00, 0x00, 0x00, // source IP address
        0x00, 0x00, 0x00, 0x00, // source IP address
        0x00, 0x00, 0x00, 0x00, // source IP address
        0x00, 0x00, 0x00, 0x00, // source IP address
        0x00, 0x00, 0x00, 0x00, // destination IP address
        0x00, 0x00, 0x00, 0x00, // destination IP address
        0x00, 0x00, 0x00, 0x00, // destination IP address
        0x00, 0x00, 0x00, 0x00, // destination IP address
    };

    uint8_t icmp_packet[] = {
        // icmp
        0x80, 0x00, 0x00, 0x00, // type: 0x80 PING Request, codde = 0, checksum(16)
        0x00, 0x00, 0x00, 0x00  // message
    };

    // ethernet header
    int pos = setup_ethernet_header(1, 0, 0, NETWORK_TYPE_IPv4); // IPv4
    
    // ipv6
    int payload_length = sizeof(icmp_packet);
    big_endian_store_16(ipv6_header, 4, payload_length);
    // TODO: also set src/dest ip address
    int checksum = calc_internet_checksum(&ipv6_header[8], 32);
    checksum = sum_ones_complement(checksum, payload_length);
    checksum = sum_ones_complement(checksum, 58 << 8);
    big_endian_store_16(icmp_packet, 2, checksum);
    memcpy(&network_buffer[pos], ipv6_header, sizeof(ipv6_header));
    pos += sizeof(ipv6_header);

    // icmp
    uint16_t icmp_checksum = calc_internet_checksum(icmp_packet, sizeof(icmp_packet));
    big_endian_store_16(icmp_packet, 2, icmp_checksum);    
    memcpy(&network_buffer[pos], icmp_packet, sizeof(icmp_packet));
    pos += sizeof(icmp_packet);

    // send
    send_buffer(pos);
}

static void send_ndp_probe_ipv6(void){

    uint8_t ipv6_header[] = {
        // ip
        0x60, 0x00, 0x00, 0x00, // version (6) + traffic class (8) + flow label (24)
        0x00, 0x00,   58, 0x01, // payload length(16), next header = IPv6-ICMP, hop limit
        0x00, 0x00, 0x00, 0x00, // source IP address
        0x00, 0x00, 0x00, 0x00, // source IP address
        0x00, 0x00, 0x00, 0x00, // source IP address
        0x00, 0x00, 0x00, 0x00, // source IP address
        0xfe, 0x80, 0x00, 0x00, // destination IP address
        0x00, 0x00, 0x00, 0x00, // destination IP address
        0x00, 0x00, 0x00, 0x00, // destination IP address
        0x00, 0x00, 0x00, 0x00, // destination IP address
    };

    uint8_t icmp_packet[] = {
        // icmp
        0x87, 0x00, 0x00, 0x00, // type: 0x80 PING Request, code = 0, checksum(16)
        0x00, 0x00, 0x00, 0x00  // message
    };

    // ethernet header
    int pos = setup_ethernet_header(1, 0, 0, NETWORK_TYPE_IPv6);

    // ipv6
    int payload_length = sizeof(icmp_packet);
    big_endian_store_16(ipv6_header, 4, payload_length);
    // source address ::
    // dest addresss - Modified EUI-64
    // ipv6_header[24..31] = FE80::
    ipv6_header[32] = local_addr[0] ^ 0x2;
    ipv6_header[33] = local_addr[1];
    ipv6_header[34] = local_addr[2];
    ipv6_header[35] = 0xff;
    ipv6_header[36] = 0xfe;
    ipv6_header[37] = local_addr[3];
    ipv6_header[38] = local_addr[4];
    ipv6_header[39] = local_addr[5];
    int checksum = calc_internet_checksum(&ipv6_header[8], 32);
    checksum = sum_ones_complement(checksum, payload_length);
    checksum = sum_ones_complement(checksum, ipv6_header[6] << 8);
    memcpy(&network_buffer[pos], ipv6_header, sizeof(ipv6_header));
    pos += sizeof(ipv6_header);

    // icmp
    uint16_t icmp_checksum = calc_internet_checksum(icmp_packet, sizeof(icmp_packet));
    big_endian_store_16(icmp_packet, 2, icmp_checksum);    
    memcpy(&network_buffer[pos], icmp_packet, sizeof(icmp_packet));
    pos += sizeof(icmp_packet);

    // send
    send_buffer(pos);    
}

static void send_llmnr_request_ipv4(void){

    uint8_t ipv4_header[] = {
        0x45, 0x00, 0x00, 0x00, // version + ihl, dscp } ecn, total len
        0x00, 0x00, 0x00, 0x00, // identification (16), flags + fragment offset
        0x01, 0x11, 0x00, 0x00, // time to live, procotol: UDP, checksum (16),
        192,   168, 167,  152,  // source IP address
        224,     0,   0,  252,  // destination IP address
    };

    uint8_t udp_header[8];
    uint8_t llmnr_packet[12];

    uint8_t dns_data[] = { 0x08, 0x61, 0x61, 0x70,  0x70, 0x6c, 0x65, 0x74, 0x76,
                           0x05, 0x6c, 0x6f, 0x63,  0x61, 0x6c, 0x00, 0x00,
                           0x01, 0x00, 0x01 }; 

    // ethernet header
    int pos = setup_ethernet_header(1, 0, 0, NETWORK_TYPE_IPv4); // IPv4

    // ipv4
    int total_length = sizeof(ipv4_header) + sizeof(udp_header) + sizeof (llmnr_packet) + sizeof(dns_data);
    big_endian_store_16(ipv4_header, 2, total_length);
    uint16_t ipv4_checksum = calc_internet_checksum(ipv4_header, sizeof(ipv4_header));
    big_endian_store_16(ipv4_header, 10, ~ipv4_checksum);    
    // TODO: also set src/dest ip address
    memcpy(&network_buffer[pos], ipv4_header, sizeof(ipv4_header));
    pos += sizeof(ipv4_header);

    // udp packet
    big_endian_store_16(udp_header, 0, 5355);   // source port
    big_endian_store_16(udp_header, 2, 5355);   // destination port
    big_endian_store_16(udp_header, 4, sizeof(udp_header) + sizeof(llmnr_packet) + sizeof(dns_data));
    big_endian_store_16(udp_header, 6, 0);      // no checksum
    memcpy(&network_buffer[pos], udp_header, sizeof(udp_header));
    pos += sizeof(udp_header);

    // llmnr packet
    memset(llmnr_packet, 0, sizeof(llmnr_packet));
    big_endian_store_16(llmnr_packet, 0, 0x1234);  // transaction id
    big_endian_store_16(llmnr_packet, 4, 1);   // one query

    memcpy(&network_buffer[pos], llmnr_packet, sizeof(llmnr_packet));
    pos += sizeof(llmnr_packet);

    memcpy(&network_buffer[pos], dns_data, sizeof(dns_data));
    pos += sizeof(dns_data);

    // send
    send_buffer(pos);
}

static void send_llmnr_request_ipv6(void){
    // https://msdn.microsoft.com/en-us/library/dd240361.aspx
    uint8_t ipv6_header[] = {
        0x60, 0x00, 0x00, 0x00, // version (6) + traffic class (8) + flow label (24)
        0x00, 0x00,   17, 0x01, // payload length(16), next header = UDP, hop limit
        0xfe, 0x80, 0x00, 0x00, // source IP address
        0x00, 0x00, 0x00, 0x00, // source IP address
        0xd9, 0xf6, 0xce, 0x2e, // source IP address
        0x48, 0x75, 0xab, 0x03, // source IP address
        0xff, 0x02, 0x00, 0x00, // destination IP address
        0x00, 0x00, 0x00, 0x00, // destination IP address
        0x00, 0x00, 0x00, 0x00, // destination IP address
        0x00, 0x01, 0x00, 0x03, // destination IP address
    };

    uint8_t udp_header[8];
    uint8_t llmnr_packet[12];

    uint8_t dns_data[] = { 0x08, 0x61, 0x61, 0x70,  0x70, 0x6c, 0x65, 0x74, 0x76,
                           0x05, 0x6c, 0x6f, 0x63,  0x61, 0x6c, 0x00, 0x00,
                           0x01, 0x00, 0x01 }; 

    int payload_length = sizeof(udp_header) + sizeof(llmnr_packet) + sizeof(dns_data);

    // llmnr header
    memset(llmnr_packet, 0, sizeof(llmnr_packet));
    big_endian_store_16(llmnr_packet, 0, 0x1235);  // transaction id
    big_endian_store_16(llmnr_packet, 4, 1);   // one query

    // ipv6 header
    big_endian_store_16(ipv6_header, 4, payload_length);

    // udp header
    memset(udp_header, 0, sizeof(udp_header));
    big_endian_store_16(udp_header, 0, 5355);   // source port
    big_endian_store_16(udp_header, 2, 5355);   // destination port
    big_endian_store_16(udp_header, 4, payload_length);
    int checksum = calc_internet_checksum(&ipv6_header[8], 32);
    checksum = sum_ones_complement(checksum, payload_length);       // payload len
    checksum = sum_ones_complement(checksum, ipv6_header[6] << 8);  // next header 
    checksum = sum_ones_complement(checksum, calc_internet_checksum(udp_header, sizeof(udp_header)));
    checksum = sum_ones_complement(checksum, calc_internet_checksum(llmnr_packet, sizeof(llmnr_packet)));
    checksum = sum_ones_complement(checksum, calc_internet_checksum(dns_data, sizeof(dns_data)));
    big_endian_store_16(udp_header, 6, ~checksum);

    // ethernet header
    int pos = setup_ethernet_header(1, 0, 1, NETWORK_TYPE_IPv6); // IPv6

    memcpy(&network_buffer[pos], ipv6_header, sizeof(ipv6_header));
    pos += sizeof(ipv6_header);

    memcpy(&network_buffer[pos], udp_header, sizeof(udp_header));
    pos += sizeof(udp_header);

    memcpy(&network_buffer[pos], llmnr_packet, sizeof(llmnr_packet));
    pos += sizeof(llmnr_packet);

    memcpy(&network_buffer[pos], dns_data, sizeof(dns_data));
    pos += sizeof(dns_data);

    // send
    send_buffer(pos);
}

static void show_usage(void){

    printf("\n--- Bluetooth BNEP Test Console ---\n");
    printf("Source        UUID %04x (== TSPX_UUID_src_address)\n", bnep_src_uuid);
    printf("Destination   UUID %04x (== TSPX_UUID_dest_address)\n", bnep_dest_uuid);
    printf("Local service UUID %04x (== TSPX_UUID_dest_address)\n", bnep_local_service_uuid);
    printf("---\n");
    printf("p - connect to PTS\n");
    printf("e - send general Ethernet packet\n");
    printf("c - send compressed Ethernet packet\n");
    printf("s - send source only compressed Ethernet packet\n");
    printf("d - send destination only compressed Ethernet packet\n");
    printf("f - set network filter\n");
    printf("m - set multicast network filter\n");
    printf("---\n");
    printf("1 - send ICMP Ping Request IPv4\n");
    printf("2 - send ICMP Ping Request IPv6\n");
    printf("4 - send IPv4 ARP request\n");
    printf("6 - send IPv6 NDP request\n");
    printf("7 - send IPv4 LLMNR request\n");
    printf("8 - send IPv6 LLMNR request\n");
    printf("---\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char c){
    switch (c){
        case 'p':
            printf("Connecting to PTS at %s...\n", bd_addr_to_str(pts_addr));
            bnep_connect(&packet_handler, pts_addr, bnep_l2cap_psm, bnep_src_uuid, bnep_dest_uuid);
            break;
        case 'e':
            printf("Sending general ethernet packet\n");
            send_ethernet_packet(0,0);
            break;
        case 'c':
            printf("Sending compressed ethernet packet\n");
            send_ethernet_packet(1,1);
            break;
        case 's':
            printf("Sending src only compressed ethernet packet\n");
            send_ethernet_packet(0,1);
            break;
        case 'd':
            printf("Sending dst only ethernet packet\n");
            send_ethernet_packet(1,0);
            break;
        case 'f':
            printf("Setting network protocol filter\n");
            set_network_protocol_filter();
            break;
        case 'm':
            printf("Setting multicast filter\n");
            set_multicast_filter();
            break;
        case '1':
            printf("Sending ICMP Ping via IPv4\n");
            send_ping_request_ipv4();
            break;
        case '2':
            printf("Sending ICMP Ping via IPv6\n");
            send_ping_request_ipv6();
            break;
        case '4':
            printf("Sending IPv4 ARP Probe\n");
            send_arp_probe_ipv4();
            break;
        case '6':
            printf("Sending IPv6 ARP Probe\n");
            send_ndp_probe_ipv6();
            break;
        case '7':
            printf("Sending IPv4 LLMNR Request\n");
            send_llmnr_request_ipv4();
            break;
        case '8':
            printf("Sending IPv6 LLMNR Request\n");
            send_llmnr_request_ipv6();
            break;
        default:
            show_usage();
            break;

    }
}

/*************** PANU client routines *********************/
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);

    uint8_t   event;
    bd_addr_t event_addr;
    bd_addr_t src_addr;
    bd_addr_t dst_addr;
    uint16_t  uuid_source;
    uint16_t  uuid_dest;
    uint16_t  mtu;    
    uint16_t  network_type;
    uint8_t   protocol_type;
    uint8_t   icmp_type;
    int       ihl;
    int       payload_offset;

    switch (packet_type) {
		case HCI_EVENT_PACKET:
            event = packet[0];
            switch (event) {            
                case BTSTACK_EVENT_STATE:
                    /* BT Stack activated, get started */ 
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        printf("BNEP Test ready\n");
                        show_usage();
                    }
                    break;

                case HCI_EVENT_COMMAND_COMPLETE:
					if (hci_event_command_complete_get_command_opcode(packet) == HCI_OPCODE_HCI_READ_BD_ADDR){
                        reverse_bd_addr(&packet[6], local_addr);
                        printf("BD-ADDR: %s\n", bd_addr_to_str(local_addr));
                        break;
                    }
                    break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%06u'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;
				
				case BNEP_EVENT_CHANNEL_OPENED:
                    if (bnep_event_channel_opened_get_status(packet)) {
                        printf("BNEP channel open failed, status %02x\n", bnep_event_channel_opened_get_status(packet));
                    } else {
                        // data: event(8), len(8), status (8), bnep source uuid (16), bnep destination uuid (16), remote_address (48)
                        bnep_cid    = bnep_event_channel_opened_get_bnep_cid(packet);
                        uuid_source = bnep_event_channel_opened_get_source_uuid(packet);
                        uuid_dest   = bnep_event_channel_opened_get_destination_uuid(packet);
                        mtu         = bnep_event_channel_opened_get_mtu(packet);
                        //bt_flip_addr(event_addr, &packet[9]); 
                        memcpy(&event_addr, &packet[11], sizeof(bd_addr_t));
                        printf("BNEP connection open succeeded to %s source UUID 0x%04x dest UUID: 0x%04x, max frame size %u\n", bd_addr_to_str(event_addr), uuid_source, uuid_dest, mtu);
                    }
					break;
                    
                case BNEP_EVENT_CHANNEL_TIMEOUT:
                    printf("BNEP channel timeout! Channel will be closed\n");
                    break;
                    
                case BNEP_EVENT_CHANNEL_CLOSED:
                    printf("BNEP channel closed\n");
                    break;

                case BNEP_EVENT_CAN_SEND_NOW:
                    /* Check for parked network packets and send it out now */
                    if (network_buffer_len > 0) {
                        bnep_send(bnep_cid, network_buffer, network_buffer_len);
                        network_buffer_len = 0;
                    }
                    break;
                    
                default:
                    break;
            }
            break;
        case BNEP_DATA_PACKET:
            // show received packet on console

            // TODO: fix BNEP to return BD ADDR in little endian, to use these lines
            // bt_flip_addr(dst_addr, &packet[0]);
            // bt_flip_addr(src_addr, &packet[6]);
            // instead of these
            memcpy(dst_addr, &packet[0], 6);
            memcpy(src_addr, &packet[6], 6);
            // END TOOD

            network_type = big_endian_read_16(packet, 12);
            printf("BNEP packet received\n");
            printf("Dst Addr: %s\n", bd_addr_to_str(dst_addr));
            printf("Src Addr: %s\n", bd_addr_to_str(src_addr));
            printf("Net Type: %04x\n", network_type);
            // ignore the next 60 bytes
            // hexdumpf(&packet[74], size - 74);
            switch (network_type){
                case NETWORK_TYPE_IPv4:
                    ihl = packet[14] & 0x0f;
                    payload_offset = 14 + (ihl << 2);
                    // protocol
                    protocol_type = packet[14 + 9]; // offset 9 into IPv4
                    switch (protocol_type){
                        case 0x01:  // ICMP
                            icmp_type = packet[payload_offset];
                            hexdumpf(&packet[payload_offset], size - payload_offset);
                            printf("ICMP packet of type %x\n", icmp_type);
                            switch (icmp_type){
                                case ICMP_V4_TYPE_PING_REQUEST:
                                    printf("IPv4 Ping Request received, sending pong\n");
                                    send_ping_response_ipv4();
                                    break;
                                break;
                            }
                        case 0x11:  // UDP
                            printf("UDP IPv4 packet\n");                        
                            hexdumpf(&packet[payload_offset], size - payload_offset);
                            break;
                        default:
                            printf("Unknown IPv4 protocol type %x", protocol_type);
                            break;
                    }
                    break;
                case NETWORK_TYPE_IPv6:
                    protocol_type = packet[6];
                    switch(protocol_type){
                        case 0x11: // UDP
                            printf("UDP IPv6 packet\n");
                            payload_offset = 40;    // fixed
                            hexdumpf(&packet[payload_offset], size - payload_offset);

                            // send response

                            break;
                        default:
                            printf("IPv6 packet of protocol 0x%02x\n", protocol_type);
                            hexdumpf(&packet[14], size - 14);
                            break;
                    }
                    break;
                default:
                    printf("Unknown network type %x", network_type);
                    break;
            }

            break;            
            
        default:
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;
    /* Register for HCI events */
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    /* Initialize L2CAP */
    l2cap_init();

    /* Initialise BNEP */
    bnep_init();
    bnep_register_service(&packet_handler, bnep_local_service_uuid, 1691);  /* Minimum L2CAP MTU for bnep is 1691 bytes */

    /* Initialize SDP and add PANU record */
    sdp_init();

    uint16_t network_packet_types[] = { NETWORK_TYPE_IPv4, NETWORK_TYPE_ARP, 0};    // 0 as end of list
    pan_create_panu_sdp_record(panu_sdp_record, 0x10002, network_packet_types, NULL, NULL, BNEP_SECURITY_NONE);
    printf("SDP service record size: %u\n", de_get_len((uint8_t*) panu_sdp_record));
    sdp_register_service((uint8_t*)panu_sdp_record);

    /* Turn on the device */
    hci_power_control(HCI_POWER_ON);
    gap_discoverable_control(1);

    btstack_stdin_setup(stdin_process);

    return 0;
}

/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-  */

