#!/usr/bin/env python3
#
# Scrape GAP Data Types from Bluetooth SIG page
# Copyright 2016 BlueKitchen GmbH
#

from lxml import html
import datetime
import re
import requests
import sys
import os

headers = {'user-agent': 'curl/7.63.0'}

program_info = '''
BTstack Data Types Scraper for BTstack
Copyright 2016, BlueKitchen GmbH
'''

header = '''/**
 * bluetooth_data_types.h generated from Bluetooth SIG website for BTstack by tool/bluetooth_data_types.py
 * {url}
 * {datetime}
 */

#ifndef BLUETOOTH_DATA_TYPES_H
#define BLUETOOTH_DATA_TYPES_H

'''

trailer = '''
#endif
'''

def clean(tag):
    # << 0xab
    # >> 0xbb
    # \n
    # non-visible whitespace 0x200b
    # non-vicible whitespace 0xa0
    return tag.replace(u'\xab','').replace(u'\xbb','').replace(u'\u200b','').replace('\n','').replace(u'\xa0',' ').strip()

def scrape_page(fout, url):
    print("Parsing %s" % url)    
    page = requests.get(url, headers=headers)
    tree = html.fromstring(page.content)

    print('')
    print('%-48s | %s' % ("Data Type Name", "Data Type Value"))
    print('-' * 70)

    # get all <tr> elements in <table id="table3">
    rows = tree.xpath('//table/tbody/tr')
    for row in rows:
        children = row.getchildren()
        data_type_value = children[0].text_content()
        data_type_name  = children[1].text_content()
        # table with references to where it was used

        if (data_type_value == 'Data Type Value'):
            continue

        # clean up
        data_type_name = clean(data_type_name)
        data_type_value = clean(data_type_value)

        tag = data_type_name
        # uppper 
        tag = tag.upper()
        # collapse ' - ' into ' '
        tag = tag.replace(' - ', ' ')
        # drop dashes otherwise
        tag = tag.replace('-',' ')
        # collect multiple spaces
        tag = re.sub('\s+', ' ', tag).strip()
        # replace space with under score
        tag =tag.replace(' ', '_')
        fout.write("#define BLUETOOTH_DATA_TYPE_%-50s %s // %s\n" %  (tag, data_type_value, data_type_name))
        print("%-48s | %s" % (data_type_name, data_type_value))

btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
gen_path = btstack_root + '/src/bluetooth_data_types.h'

print(program_info)

with open(gen_path, 'wt') as fout:
    url = 'https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile'
    fout.write(header.format(datetime=str(datetime.datetime.now()), url=url.replace('https://','')))
    scrape_page(fout, url)
    fout.write(trailer)

print('')
print('Scraping successful into %s!\n' % gen_path)
