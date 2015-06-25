#!/usr/bin/env python
import os, sys, getopt, re

class State:
    SearchStartAPI = 0
    RemoveEmptyLinesAfterAPIStart = 1
    SearchEndAPI = 2
    DoneAPI = 3

api_header = """

## API_TITLE API {#sec:API_LABLEAPIAppendix}

"""

api_ending = """
"""


def replacePlaceholder(template, title, lable):
    api_title = title + " API"
    snippet = template.replace("API_TITLE", title).replace("API_LABLE", lable)
    return snippet

def writeAPI(fout, api_filename, mk_codeidentation):
    state = State.SearchStartAPI
    with open(api_filename, 'rb') as fin:
        for line in fin:
            if state == State.SearchStartAPI:
                parts = re.match('\s*(/\*).*API_START.*(\*/)',line)
                if parts:
                    state = State.RemoveEmptyLinesAfterAPIStart
                    continue
            
            if state == State.RemoveEmptyLinesAfterAPIStart:
                if line == "" or line == "\n":
                    continue
                state = State.SearchEndAPI

            if state == State.SearchEndAPI:
                parts = re.match('\s*(/\*).*API_END.*(\*/)',line)
                if parts:
                    state = State.DoneAPI
                    return
                fout.write(mk_codeidentation + line)
        
def main(argv):
    btstackfolder = "../../"
    docsfolder    = "docs/"
    mk_codeidentation = "    "
    
    outputfile  = docsfolder + "appendix/apis.md"
    
    cmd = 'update_apis.py [-b <btstackfolder>] [-o <outputfile>]'
    try:
        opts, args = getopt.getopt(argv,"hiso:",["bfolder=","ofile="])
    except getopt.GetoptError:
        print cmd
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print cmd
            sys.exit()
        elif opt in ("-b", "--bfolder"):
            btstackfolder = arg
        elif opt in ("-o", "--ofile"):
            outputfile = arg
    print 'BTstack folder is ', btstackfolder
    print 'Output file is ', outputfile

    
    # [file_name, api_title, api_lable]
    list_of_apis = [ 
        [btstackfolder+"include/btstack/run_loop.h", "Run Loop", "runLoop"],
        [btstackfolder+"src/hci.h", "HCI", "hci"],
        [btstackfolder+"src/l2cap.h", "L2CAP", "l2cap"],
        [btstackfolder+"src/rfcomm.h", "RFCOMM", "rfcomm"],
        [btstackfolder+"src/sdp.h", "SDP", "sdp"],
        [btstackfolder+"src/sdp_client.h", "SDP Client", "sdpClient"],
        [btstackfolder+"src/sdp_query_rfcomm.h", "SDP RFCOMM Query", "sdpQueries"],
        [btstackfolder+"ble/gatt_client.h", "GATT Client", "gattClient"],
        [btstackfolder+"src/pan.h", "PAN", "pan"],
        [btstackfolder+"src/bnep.h", "BNEP", "bnep"],
        [btstackfolder+"src/gap.h", "GAP", "gap"],
        [btstackfolder+"ble/sm.h", "SM", "sm"]
    ]

    with open(outputfile, 'w') as fout:
        for api_tuple in list_of_apis:
            api_filename = api_tuple[0]
            api_title = api_tuple[1]
            api_lable = api_tuple[2]

            fout.write(replacePlaceholder(api_header, api_title, api_lable))
            writeAPI(fout, api_filename, mk_codeidentation)


if __name__ == "__main__":
   main(sys.argv[1:])
