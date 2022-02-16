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

import csv
import getopt


program_info = '''
BTstack Company ID Scraper for BTstack
Copyright 2022, BlueKitchen GmbH
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
    tag = tag.replace(' ','_')
    tag = tag.replace('&','AND')
    tag = tag.replace("'","_")
    tag = tag.replace('"','_')
    tag = tag.replace('!','_')
    tag = tag.replace('|','_')
    tag = tag.replace('[','')
    tag = tag.replace(']','')
    return "BLUETOOTH_COMPANY_ID_" + tag

def parse_cvs(csv_file):
    cvsreader = csv.reader(csv_file, delimiter=',', quotechar='\"')
    # Skip header ['"Decimal","Hexadecimal","Company"']
    next(cvsreader)
    
    companies = {}
    for row in cvsreader:
        id_dec = int(row[0])
        companies[id_dec] = (row[1],row[2])
    return companies
    
   
def write_cvs(fout, companies, url):
    global tags
    fout.write(page_info.format(page=url.replace('https://','')))

    company_ids = sorted(list(companies.keys()))

    for id_dec in company_ids:
        id_hex = companies[id_dec][0]
        company = create_name(companies[id_dec][1])
        
        if company in tags:
            company = company+"2"
        else:
            tags.append(company)

        fout.write("#define %-80s %s\n" %  (company, id_hex))

    # map CSR onto QTIL
    fout.write("#define BLUETOOTH_COMPANY_ID_CAMBRIDGE_SILICON_RADIO BLUETOOTH_COMPANY_ID_QUALCOMM_TECHNOLOGIES_INTERNATIONAL_LTD\n")

def main(argv):
    url = "https://www.bluetooth.com/de/specifications/assigned-numbers/company-identifiers/#"
    btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
    tools_root = btstack_root + '/tool'
    src_root   = btstack_root + '/src/'
    
    header_filename = "bluetooth_company_id.h"
    header_path = btstack_root + "/src/" + header_filename
    
    cvs_filename = "CompanyIdentfiers - CSV.csv"
    cvs_path = tools_root + "/" + cvs_filename
    
    print(program_info)

    try:
        header_file = open(header_path, "w")
    except FileNotFoundError:
        print("\nFile \'%s\' cannot be created in \'%s\' directory." % (header_filename, src_root))
        exit(1)
        
    try:
        with open(cvs_path, "r") as csv_file:
            companies = parse_cvs(csv_file)

            header_file.write(header.format(datetime=str(datetime.datetime.now())))
            write_cvs(header_file, companies, url)
            header_file.write(trailer)

            print("Company IDs are stored in \'%s\'\n" % (header_path))
    except FileNotFoundError:
        print("\nCVS file \'%s\' not found in \'%s\'." % (cvs_filename, tools_root))
        print("Please download the CVS file, then start the skript again.\n")
        print("The CVS file can be downloaded from:")
        print("     %s\n" % url)
        exit(1)

if __name__ == "__main__":
   main(sys.argv[1:])


