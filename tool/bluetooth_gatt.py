#!/usr/bin/env python3
#
# Scrape GATT UUIDs from Bluetooth SIG page
# Copyright 2016 BlueKitchen GmbH
#

from lxml import html
import datetime
import requests
import sys
import os

headers = {'user-agent': 'curl/7.63.0'}

program_info = '''
BTstack GATT UUID Scraper for BTstack
Copyright 2016, BlueKitchen GmbH
'''

header = '''
/**
 * bluetooth_gatt.h generated from Bluetooth SIG website for BTstack tool/bluetooth_gatt.py
 * {datetime}
 */

#ifndef BLUETOOTH_GATT_H
#define BLUETOOTH_GATT_H
'''

page_info = '''
/**
 * Assigned numbers from {page}
 */
'''

trailer = '''
#endif
'''

def strip_non_ascii(string):
    stripped = (c for c in string if 0 < ord(c) < 127)
    return ''.join(stripped)

def scrape_page(fout, url):
    print("Parsing %s" % url)    
    fout.write(page_info.format(page=url.replace('https://','')))
    page = requests.get(url, headers=headers)
    tree = html.fromstring(page.content)
    # get all <tr> elements in <table>
    rows = tree.xpath('//table/tbody/tr')
    for row in rows:
        children = row.getchildren()
        summary = strip_non_ascii(children[0].text_content())
        id      = children[1].text_content()
        # fix unexpected suffix _
        id = id.replace('.gatt_.', '.gatt.')
        uuid    = children[2].text_content()
        if (len(id)):
            tag = id.upper().replace('.', '_').replace('-','_')
            fout.write("#define %-80s %s // %s\n" %  (tag, uuid, summary))

btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
gen_path = btstack_root + '/src/bluetooth_gatt.h'

print(program_info)

with open(gen_path, 'wt') as fout:
    fout.write(header.format(datetime=str(datetime.datetime.now())))
    scrape_page(fout, 'https://www.bluetooth.com/specifications/gatt/declarations')
    scrape_page(fout, 'https://www.bluetooth.com/specifications/gatt/services')
    scrape_page(fout, 'https://www.bluetooth.com/specifications/gatt/characteristics')
    scrape_page(fout, 'https://www.bluetooth.com/specifications/gatt/descriptors')
    fout.write("// START(manually added, missing on Bluetooth Website\n")
    fout.write("#define %-80s %s // %s\n" %  ("ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROVISIONING_DATA_IN" , "0x2ADB", ''))
    fout.write("#define %-80s %s // %s\n" %  ("ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROVISIONING_DATA_OUT", "0x2ADC", ''))
    fout.write("#define %-80s %s // %s\n" %  ("ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROXY_DATA_IN"        , "0x2ADD", ''))
    fout.write("#define %-80s %s // %s\n" %  ("ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROXY_DATA_OUT"       , "0x2ADE", ''))
    fout.write("// END(manualy added, missing on Bluetooth Website\n")
    fout.write(trailer)

print('Scraping successful!\n')
