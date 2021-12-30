#!/usr/bin/env python3
#
# Convert format UUID16 // SERVICE_NAME into bluetooth_gatt.h defines
# Copyright 2021 BlueKitchen GmbH
#
import os, sys, getopt

tag_description = {
    "-c" : "ORG_BLUETOOTH_CHARACTERISTIC",
    "-s" : "ORG_BLUETOOTH_SERVICE"
}

def main(argv):
    cmd = "\nUSAGE: %s [-s|-c] [-f filename]" % sys.argv[0]
    cmd += "\n -s: for SERVICE_UUID"
    cmd += "\n -c: for CHARACTERISTICS_UUID"
    cmd += "\n -f filename: input file with UUID and comment, i.e. 0x2B29 // Client Supported Features\n"
    
    tag_define = None
    filename = None

    try:
        opts, args = getopt.getopt(argv[1:],"scf:")
    except getopt.GetoptError:
        print("ERROR: wrong options")
        print (cmd)
        sys.exit(2)
    
    print(opts)

    for opt, arg in opts:
        if opt == '-s' or opt == '-c':
            tag_define = tag_description[opt]
        elif opt == '-f':
            print("filename")
            filename = arg
        else:
            print("ERROR: wrong options")
            print (cmd)
            sys.exit(2)

    if (not tag_define) or (not filename):
        print("ERROR: wrong options")
        print (cmd)
        sys.exit(2)

    with open (filename, 'rt') as fin:
        for line in fin:
            data = line.strip('\n').split(" // ")
            if len(data) != 2:
                continue
            else:
                uuid = data[0]
                summary = data[1]

                tag = summary.upper().replace('.', '_').replace('-','_').replace(' ', '_')
                print("#define %s_%-80s %s // %s" %  (tag_define, tag, uuid, summary))

    

if __name__ == "__main__":
   main(sys.argv)
        