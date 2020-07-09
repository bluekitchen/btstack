#!/usr/bin/env python3
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

headers = {'user-agent': 'curl/7.63.0'}

program_info = '''
BTstack Company ID Scraper for BTstack
Copyright 2017, BlueKitchen GmbH
'''

header = '''
/**
 * bluetooth_company_id.h generated from Bluetooth SIG website for BTstack by tool/bluetooth_company_id.py
 * {datetime}
 */

#ifndef BLUETOOTH_COMPANY_ID_H
#define BLUETOOTH_COMPANY_ID_H
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

def strip_non_ascii(string):
    stripped = (c for c in string if 0 < ord(c) < 127)
    return ''.join(stripped)

def create_name(company):
    # limit to ascii
    company = strip_non_ascii(company)
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
    tag = tag.replace('&','AND')
    tag = tag.replace("'","_")
    tag = tag.replace('"','_')
    tag = tag.replace('!','_')
    return "BLUETOOTH_COMPANY_ID_" + tag

def scrape_page(fout, url):
    global headers

    print("Parsing %s" % url)    
    fout.write(page_info.format(page=url.replace('https://','')))

    # get from web
    r = requests.get(url, headers=headers)
    content = r.text

    # test: fetch from local file 'service-discovery.html'
    # f = codecs.open("company-identifiers.html", "r", "utf-8")
    # content = f.read();

    tree = html.fromstring(content)
    rows = tree.xpath('//table/tbody/tr')
    for row in rows:
        children = row.getchildren()
        id_hex  = children[1].text_content()
        company = create_name(children[2].text_content())
        if company in tags:
            company = company+"2"
        else:
            tags.append(company)
        fout.write("#define %-80s %s\n" %  (company, id_hex))

    # map CSR onto QTIL
    fout.write("#define BLUETOOTH_COMPANY_ID_CAMBRIDGE_SILICON_RADIO BLUETOOTH_COMPANY_ID_QUALCOMM_TECHNOLOGIES_INTERNATIONAL_LTD\n")

btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
gen_path = btstack_root + '/src/bluetooth_company_id.h'

print(program_info)

with open(gen_path, 'wt') as fout:
    fout.write(header.format(datetime=str(datetime.datetime.now())))
    scrape_page(fout, 'https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers')
    fout.write(trailer)

print('Scraping successful!\n')
