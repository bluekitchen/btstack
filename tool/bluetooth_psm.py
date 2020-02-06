#!/usr/bin/env python
#
# Scrape GATT UUIDs from Bluetooth SIG page
# https://www.bluetooth.com/specifications/assigned-numbers/logical-link-control/
#
# Copyright 2019 BlueKitchen GmbH
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
BTstack PSM Scraper
Copyright 2019, BlueKitchen GmbH
'''

header = '''
/**
 * bluetooth_psm.h generated from Bluetooth SIG website for BTstack by tool/bluetooth_psm.py
 * {datetime}
 */

#ifndef BLUETOOTH_PSM_H
#define BLUETOOTH_PSM_H
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

def create_name(psm):
    # limit to ascii
    psm = strip_non_ascii(psm)
    # remove parts in braces
    p = re.compile('\(.*\)')
    tag = p.sub('',psm).rstrip().upper()
    tag = tag.replace('-', '_')
    return "BLUETOOTH_PSM_" + tag

def scrape_page(fout, url):
    global headers

    print("Parsing %s" % url)    
    fout.write(page_info.format(page=url.replace('https://','')))

    # get from web
    r = requests.get(url, headers=headers)
    content = r.text

    # test: fetch from local file 'index.html'
    # f = codecs.open("index.html", "r", "utf-8")
    # content = f.read();

    tree = html.fromstring(content)
    rows = tree.xpath('//table/tbody/tr')
    for row in rows:
        children = row.getchildren()
        psm      = children[0].text_content()

        # abort when second table starts
        if (psm == '0x0000-0xFFFF'):
            break

        id_hex   = children[1].text_content().replace(u'\u200b','')
        fout.write("#define %-80s %s\n" %  (create_name(psm), id_hex))

btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
gen_path = btstack_root + '/src/bluetooth_psm.h'

print(program_info)

with open(gen_path, 'wt') as fout:
    fout.write(header.format(datetime=str(datetime.datetime.now())))
    scrape_page(fout, 'https://www.bluetooth.com/specifications/assigned-numbers/logical-link-control/')
    fout.write(trailer)

print('Scraping successful!\n')
