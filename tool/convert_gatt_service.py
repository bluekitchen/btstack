#!/usr/bin/env python
#
# Convert offical Bluetooth GATT Service definitions into BTstack's .gatt format
# Copyright 2016 BlueKitchen GmbH
#

from lxml import html
import codecs
import datetime
import os
import requests
import sys
import xml.etree.ElementTree as ET

headers = {'user-agent': 'curl/7.63.0'}

def indent(elem, level=0):
    i = "\n" + level*"  "
    j = "\n" + (level-1)*"  "
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "  "
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
        for subelem in elem:
            indent(subelem, level+1)
        if not elem.tail or not elem.tail.strip():
            elem.tail = j
    else:
        if level and (not elem.tail or not elem.tail.strip()):
            elem.tail = j
    return elem  

def list_services():
    global headers

    url     = 'https://www.bluetooth.com/specifications/gatt/services'
    print("Fetching list of services from %s" % url)
    print('')

    page = requests.get(url, headers=headers)

    tree = html.fromstring(page.content)
    # get all <tr> elements in <table">
    rows = tree.xpath('//table/tbody/tr')
    print("%-55s| %-30s| %s" % ('Specification Type', 'Specification Name', 'UUID'))
    print('-'*55 + '+-' + '-' * 30 + '+-' + '-'*10)
    maxlen_type = 0
    maxlen_name = 0
    services = []
    for row in rows:
        children = row.getchildren()
        summary = children[0].text_content()
        id      = children[1].text_content()
        uuid    = children[2].text_content()
        if (len(id)):
            services.append((id, summary, uuid))
    # sort
    services.sort(key=lambda tup: tup[1])
    for service in services:
        if (len(id) > maxlen_type):
            maxlen_type = len(id)
        if (len(summary) > maxlen_name):
            maxlen_name = len(summary)
        print("%-55s| %-30s| %s" % service)

def parse_properties(element):
    properties = element.find('Properties')
    property_list_human = []
    for property in properties:
        property_name = property.tag
        if property_name == "WriteWithoutResponse":
            property_name = "Write_Without_Response"
        if property_name == "InformationText":
            continue
        property_requirement = property.text
        if (property_requirement == "Excluded"):
            continue
        property_list_human.append(property_name)
    return (property_list_human)

    print("Characteristic %s - properties %s" % (name, property_list_human))
    fout.write('CHARACTERISTIC, %s, %s,\n' % (type, ' | '.join(property_list)))

def handle_todo(fout, name):
    print('-- Descriptor %-40s - TODO: Please set values' % name)
    fout.write('// TODO: %s: please set values\n' % name)

def define_for_type(type):
    return type.upper().replace('.','_')

def convert_service(fout, specification_type):
    global headers

    url = 'https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Services/%s.xml' % specification_type
    print('Fetching %s from %s' %(specification_type, url))

    try:
        page = requests.get(url, headers=headers)
        print(page.content)
        tree = ET.fromstring(page.content)
        service_attributes = tree.attrib
    except ET.ParseError:
        print('Failed to download file')
        sys.exit(10)

    print('')

    # indent(tree)
    # ET.dump(tree)
    # return

    service_name = service_attributes['name']
    service_uuid = service_attributes['uuid']

    print("Service %s" % service_name)
    fout.write('// Specification Type %s\n' % specification_type)
    fout.write('// %s\n' % url)
    fout.write('\n')
    fout.write('// %s %s\n' % (service_name, service_uuid))
    fout.write('PRIMARY_SERVICE, %s\n' % define_for_type(specification_type)) 

    characteristics = tree.find('Characteristics')
    for characteristic in characteristics:
        type = characteristic.attrib['type']
        name = characteristic.attrib['name']
        property_list_human = parse_properties(characteristic)
        properties = ' | '.join( ['DYNAMIC'] + property_list_human).upper()
        print("- Characteristic %s - properties %s" % (name, property_list_human))
        fout.write('CHARACTERISTIC, %s, %s,\n' % (define_for_type(type), properties))

        descriptors = characteristic.find('Descriptors')
        if descriptors is None:
            continue

        for descriptor in descriptors:
            type = descriptor.attrib['type']
            name = descriptor.attrib['name']
            property_list_human = parse_properties(descriptor)
            properties = ' | '.join(property_list_human).upper()

            if (type == 'org.bluetooth.descriptor.gatt.client_characteristic_configuration'):
                print('-- Descriptor %-40s' % name)
                fout.write('CLIENT_CHARACTERISTIC_CONFIGURATION, %s,\n' % properties)
                continue

            if (type == 'org.bluetooth.descriptor.gatt.server_characteristic_configuration' or
                type == 'org.bluetooth.descriptor.server_characteristic_configuration'):
                print('-- Descriptor %-40s' % name)
                fout.write('SERVER_CHARACTERISTIC_CONFIGURATION, %s,\n' % properties)
                continue

            if (type == 'org.bluetooth.descriptor.gatt.characteristic_presentation_format'):
                handle_todo(fout, 'Characteristic Presentation Format')
                fout.write('#TODO CHARACTERISTIC_FORMAT, %s, _format_, _exponent_, _unit_, _name_space_, _description_\n' % properties)
                continue

            if (type == 'org.bluetooth.descriptor.gatt.characteristic_user_description'):
                handle_todo(fout, 'Characteristic User Description')
                fout.write('#TODO CHARACTERISTIC_USER_DESCRIPTION, %s, _user_description_\n' % properties)
                continue

            if (type == 'org.bluetooth.descriptor.gatt.characteristic_aggregate_format'):
                handle_todo(fout, 'Characteristic Aggregate Format')
                fout.write('#TODO CHARACTERISTIC_AGGREGATE_FORMAT, %s, _list_of_handles_\n' % properties)
                continue

            if (type == 'org.bluetooth.descriptor.valid_range'):
                print('-- Descriptor %-40s - WARNING: VALID_RANGE not supported yet' % name)
                continue

            if (type == 'org.bluetooth.descriptor.external_report_reference'):
                print('-- Descriptor %-40s - WARNING: EXTERNAL_REPORT_REFERENCE not supported yet' % name)
                continue

            if (type == 'org.bluetooth.descriptor.report_reference'):
                print('-- Descriptor %-40s' % name)
                fout.write('REPORT_REFERENCE, %s,\n' % properties)
                continue

            if (type == 'org.bluetooth.descriptor.number_of_digitals'):
                print('-- Descriptor %-40s - WARNING: NUMBER_OF_DIGITALS not supported yet' % name)
                continue

            if (type == 'org.bluetooth.descriptor.value_trigger_setting'):
                print('-- Descriptor %-40s - WARNING: VALUE_TRIGGER_SETTING not supported yet' % name)
                continue

            if (type == 'org.bluetooth.descriptor.es_configuration'):
                print('-- Descriptor %-40s - WARNING: ENVIRONMENTAL_SENSING_CONFIGURATION not supported yet' % name)
                continue

            if (type == 'org.bluetooth.descriptor.es_measurement'):
                print('-- Descriptor %-40s - WARNING: ENVIRONMENTAL_SENSING_MEASUREMENT not supported yet' % name)
                continue

            if (type == 'org.bluetooth.descriptor.es_trigger_setting'):
                print('-- Descriptor %-40s - WARNING: ENVIRONMENTAL_SENSING_TRIGGER_SETTING not supported yet' % name)
                continue

            if (type == 'org.bluetooth.descriptor.gatt.characteristic_extended_properties'):
                print('-- Descriptor %-40s - WARNING: not supported yet' % name)
                continue

            print('-- Descriptor %-40s -- WARNING: not supported yet %s' % (name, type))


if (len(sys.argv) < 2):
    list_services()
    print('\n')
    print('To convert a service into a .gatt file template, please call the script again with the requested Specification Type and the output file name')
    print('Usage: %s SPECIFICATION_TYPE [service_name.gatt]' % sys.argv[0])
    print('')
else:
    specification_type = sys.argv[1]
    filename = '%s.gatt' % specification_type
    if (len(sys.argv)>=3):
        filename = sys.argv[2]
    with open(filename, 'wt') as fout:
        convert_service(fout, specification_type)
        print('')
        print('Service successfully created %s' % filename)
        print('Please check for TODOs in the .gatt file')
