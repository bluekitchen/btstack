#!/usr/bin/env python
#
# Scrape GATT UUIDs from Bluetooth SIG page
# Copyright 2016 BlueKitchen GmbH
#

from lxml import html
import datetime
import requests
import sys
import os

program_info = '''
BTstack GATT UUID Scraper for BTstack
Copyright 2016, BlueKitchen GmbH
'''

header = '''
/**
 * bluetooth_gatt.h generated from Bluetooth SIG website for BTstack
 */

#ifndef __BLUETOOTH_GATT_H
#define __BLUETOOTH_GATT_H
'''

page_info = '''
/**
 * Assigned numbers from {page}
 */
'''

trailer = '''
#endif
'''

def scrape_page(fout, url):
    print("Parsing %s" % url)    
    fout.write(page_info.format(page=url))
    page = requests.get(url)
    tree = html.fromstring(page.content)
    # get all <tr> elements in <table id="gattTable">
    rows = tree.xpath('//table[@id="gattTable"]/tbody/tr')
    for row in rows:
        children = row.getchildren()
        summary = children[0].text_content()
        id      = children[1].text_content()
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
    fout.write(trailer)

print('Scraping successful!\n')