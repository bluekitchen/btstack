#!/usr/bin/env python
#
# Scrape SDP UUIDs from Bluetooth SIG page
# Copyright 2017 BlueKitchen GmbH
#

from lxml import html
import datetime
import requests
import sys
import os
import codecs
import re

program_info = '''
BTstack SDP UUID Scraper for BTstack
Copyright 2017, BlueKitchen GmbH
'''

header = '''/**
 * bluetooth_sdp.h generated from Bluetooth SIG website for BTstack by tool/bluetooth_sdp.py
 * {page}
 */

#ifndef __BLUETOOTH_SDP_H
#define __BLUETOOTH_SDP_H

'''

trailer = '''
#endif
'''

defines = []

# Convert CamelCase to snake_case from http://stackoverflow.com/a/1176023
def camel_to_underscore(name):
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).upper()

def create_pretty_define(name):
    name = name.lstrip()
    to_delete = [ '(FTP v1.2 and later)', '(Deprecated)', '(FTP v1.2 and later)', '(GOEP v2.0 and later)',
     '(BIP v1.1 and later)', '(MAP v1.2 and later)', '(OPP v1.2 and later)', '(Not used in PAN v1.0)', '(PBAP v1.2 and later)']
    for item in to_delete:
        name = name.replace(item, '')
    name = name.rstrip()
    name = name.replace(' - ', '_')
    name = name.replace(' ', '_')
    name = name.replace('/','')
    name = name.replace('(','_')
    name = name.replace(')','')
    name = name.replace('-','_')
    name = name.replace('PnP', 'PNP')
    name = name.replace('IPv', 'IPV')
    name = name.replace('ServiceDiscoveryServerServiceClassID', 'ServiceDiscoveryServer')
    name = name.replace('BrowseGroupDescriptorServiceClassID', 'BrowseGroupDescriptor')
    name = name.replace('&','and')
    return camel_to_underscore(name).replace('__','_').replace('3_D','3D').replace('L2_CAP','L2CAP')

def remove_newlines(remark):
    return " ".join(remark.split())

def process_table(fout, tbody, pattern):
    rows = tbody.getchildren()
    for row in rows:
        columns = row.getchildren()
        name = columns[0].text_content().encode('ascii','ignore')
        value = columns[1].text_content().encode('ascii','ignore')
        remark = ''
        if (len(columns) > 2):
            remark = columns[2].text_content().encode('ascii','ignore')
        # skip tbody headers
        if name in ["Protocol Name", "Service Class Name", "Attribute Name", "UUID Name", 
            "Reserved", 'Reserved for HID Attributes', 'Available for HID Language Strings']:
            continue
        # skip tbody footers
        if value.startswith('(Max value '):
            continue 
        name = create_pretty_define(name)
        # skip duplicate attributes
        if name in defines:
            continue
        value = remove_newlines(value)
        remark = remove_newlines(remark)
        fout.write(pattern % (name, value, remark))
        defines.append(name)

def scrape_attributes(fout, tree, table_name):
    tables = tree.xpath("//table[preceding-sibling::h3 = '" + table_name +"']")
    tbody = tables[0].getchildren()[0]
    process_table(fout, tbody, '#define BLUETOOTH_ATTRIBUTE_%-54s %s // %s\n')

def scrape_page(fout, url):
    print("Parsing %s" % url)    
    fout.write(header.format(page=url))

    # get from web
    r = requests.get(url)
    content = r.text

    # test: fetch from local file 'service-discovery.html'
    # f = codecs.open("service-discovery.html", "r", "utf-8")
    # content = f.read();

    tree = html.fromstring(content)

    # # Protocol Identifiers
    fout.write('/**\n')
    fout.write(' * Protocol Identifiers\n')
    fout.write(' */\n')
    tables = tree.xpath("//table[preceding-sibling::h3 = 'Protocol Identifiers']")
    tbody = tables[0].getchildren()[0]
    process_table(fout, tbody, '#define BLUETOOTH_PROTOCOL_%-55s %s // %s\n')
    fout.write('\n')

    # # Service Classes
    fout.write('/**\n')
    fout.write(' * Service Classes\n')
    fout.write(' */\n')
    tables = tree.xpath("//table[preceding-sibling::h3 = 'Protocol Identifiers']")
    tbody = tables[1].getchildren()[0]
    process_table(fout, tbody, '#define BLUETOOTH_SERVICE_CLASS_%-50s %s // %s\n')
    fout.write('\n')

    # Attributes
    fout.write('/**\n')
    fout.write(' * Attributes\n')
    fout.write(' */\n')
    table_names = [
        # 'Base Universally Unique Identifier (UUID)',
        'Browse Group Identifiers',
        'Attribute Identifiers',
        # 'Audio/Video Remote Control Profile (AVRCP)',
        'Basic Imaging Profile (BIP)',
        'Basic Printing Profile (BPP)',
        'Bluetooth Core Specification: Universal Attributes',
        'Bluetooth Core Specification: Service Discovery Service',
        # 'Bluetooth Core Specification: Browse Group Descriptor Service',
        # 'Cordless Telephony Profile [DEPRECATED]',
        'Device Identification Profile',
        # 'Fax Profile [DEPRECATED]',
        'File Transfer Profile',
        'Generic Object Exchange Profile',
        # 'Global Navigation Satellite System Profile (GNSS)', -- note: SupportedFeatures, but different UUID
        'Hands-Free Profile',
        'Hardcopy Replacement Profile ',
        'Headset Profile',
        'Health Device Profile',
        'Human Interface Device Profile',
        # 'Interoperability Requirements for Bluetooth technology as a WAP Bearer [DEPRECATED]',
        'Message Access Profile',
        'Object Push Profile',
        'Personal Area Networking Profile',
        'Phone Book Access Profile',
        'Synchronization Profile',
        # 'Attribute ID Offsets for Strings',
        # 'Protocol Parameters',
        'Multi-Profile',
        'Calendar Tasks and Notes',
    ]
    for table_name in table_names:
        scrape_attributes(fout, tree, table_name)
    # see above
    fout.write('#define BLUETOOTH_ATTRIBUTE_GNSS_SUPPORTED_FEATURES                                0x0200\n');
    


btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
gen_path = btstack_root + '/src/bluetooth_sdp.h'

print(program_info)

with open(gen_path, 'wt') as fout:
    scrape_page(fout, 'https://www.bluetooth.com/specifications/assigned-numbers/service-discovery')
    fout.write(trailer)

print('Scraping successful!\n')