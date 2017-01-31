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

header = '''
/**
 * bluetooth_sdp.h generated from Bluetooth SIG website for BTstack
 */

#ifndef __BLUETOOTH_SDP_H
#define __BLUETOOTH_SDP_H
'''

page_info = '''
/**
 * Assigned numbers from {page}
 */
'''

trailer = '''
#endif
'''

# Convert CamelCase to snake_case from http://stackoverflow.com/a/1176023
def camel_to_underscore(name):
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).upper()

def create_pretty_define(name):
    name = name.replace(' - ', '_')
    name = name.replace(' ', '_')
    name = name.replace('/','')
    name = name.replace('(','_')
    name = name.replace(')','')
    name = name.replace('-','_')
    name = name.replace('PnP', 'PNP')
    return camel_to_underscore(name).replace('__','_').replace('3_D','3D').replace('L2_CAP','L2CAP')

def clean_remark(remark):
    return " ".join(remark.split())

def process_table(fout, tbody, pattern):
    rows = tbody.getchildren()
    for row in rows:
        columns = row.getchildren()
        name = columns[0].text_content().encode('ascii','ignore')
        value = columns[1].text_content().encode('ascii','ignore')
        remark = columns[2].text_content().encode('ascii','ignore')
        # skip tbody headers
        if name == "Protocol Name":
            continue
        if name == "Service Class Name":
            continue
        # skip tbody footers
        if value.startswith('(Max value '):
            continue 
        name = create_pretty_define(name)
        remark = clean_remark(remark)
        fout.write(pattern % (name, value, remark))
        print("'%s' = '%s' -- %s" % (name, value, remark))
    fout.write('\n')

def scrape_page(fout, url):
    print("Parsing %s" % url)    

    fout.write(page_info.format(page=url))

    # get from web
    # r = requests.get(url)
    # content = r.text

    # test: fetch from local file 'service-discovery.html'
    f = codecs.open("service-discovery.html", "r", "utf-8")
    content = f.read();

    tree = html.fromstring(content)

    # Protocol Identifiers
    fout.write('//\n')
    fout.write('// Protocol Identifiers\n')
    fout.write('//\n')
    tables = tree.xpath("//table[preceding-sibling::h3 = 'Protocol Identifiers']")
    tbody = tables[0].getchildren()[0]
    process_table(fout, tbody, '#define BLUETOOTH_PROTOCOL_%-55s %s // %s\n')

    # Service Classes
    fout.write('//\n')
    fout.write('// Service Classes\n')
    fout.write('//\n')
    tables = tree.xpath("//table[preceding-sibling::h3 = 'Protocol Identifiers']")
    tbody = tables[1].getchildren()[0]
    process_table(fout, tbody, '#define BLUEROOTH_SERVICE_CLASS_%-50s %s // %s\n')

btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
gen_path = btstack_root + '/src/bluetooth_sdp.h'

print(program_info)

with open(gen_path, 'wt') as fout:
    fout.write(header.format(datetime=str(datetime.datetime.now())))
    scrape_page(fout, 'https://www.bluetooth.com/specifications/assigned-numbers/service-discovery')
    fout.write(trailer)

print('Scraping successful!\n')