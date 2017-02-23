#!/usr/bin/env python
#
# Scrape GATT UUIDs from Bluetooth SIG page
# https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
#
# Copyright 2017 BlueKitchen GmbH
#

from lxml import html
import datetime
import requests
import sys
import codecs
import os
import re

program_info = '''
BTstack Company ID Scraper for BTstack
Copyright 2017, BlueKitchen GmbH
'''

header = '''
/**
 * bluetooth_company_id.h generated from Bluetooth SIG website for BTstack
 */

#ifndef __BLUETOOTH_COMPANY_ID_H
#define __BLUETOOTH_COMPANY_ID_H
'''

page_info = '''
/**
 * Assigned numbers from {page}
 */
'''

trailer = '''
#endif
'''

tags = []

def create_name(company):
    # remove parts in braces
    p = re.compile('\(.*\)')
    tag = p.sub('',company).rstrip().upper()
    tag = tag.replace('&AMP;',' AND ')
    tag = tag.replace('&#39;','')
    tag = tag.replace('&QUOT;',' ')
    tag = tag.replace('+',' AND ')
    tag = tag.replace(' - ', ' ')
    tag = tag.replace('/', ' ')
    tag = tag.replace(';',' ')
    tag = tag.replace(',','')
    tag = tag.replace('.', '')
    tag = tag.replace('-','_')
    tag = tag.replace('  ',' ')
    tag = tag.replace('  ',' ')
    tag = tag.replace('  ',' ')
    tag = tag.replace(' ','_')
    return "BLUETOOTH_COMPANY_ID_" + tag

def scrape_page(fout, url):
    print("Parsing %s" % url)    
    fout.write(page_info.format(page=url))

    # get from web
    r = requests.get(url)
    content = r.text

    # test: fetch from local file 'service-discovery.html'
    # f = codecs.open("company-identifiers.html", "r", "utf-8")
    # content = f.read();

    tree = html.fromstring(content)
    # get all java script
    rows = tree.xpath('//script')
    for row in rows:
        script = row.text_content()
        if not 'DataTable' in script:
            continue
        start_tag = 'data:  ['
        end_tag   = '["65535","0xFFFF",'
        start = script.find(start_tag)
        end   = script.find(end_tag)
        company_list = script[start + len(start_tag):end]
        for entry in company_list.split('],'):
            if len(entry) < 5:
                break
            entry = entry[1:]
            fields = entry.split('","')
            id_hex = fields[1]
            company = create_name(fields[2][:-1])
            if company in tags:
                company = company + "2"
            else:
                tags.append(company)
            if len(company) < 2:
                continue
            fout.write("#define %-80s %s\n" %  (company, id_hex))

btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
gen_path = btstack_root + '/src/bluetooth_company_id.h'

print(program_info)

with open(gen_path, 'wt') as fout:
    fout.write(header.format(datetime=str(datetime.datetime.now())))
    scrape_page(fout, 'https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers')
    fout.write(trailer)

print('Scraping successful!\n')