#!/usr/bin/env python
import os
import re
import sys 

class State:
    SearchStartAPI = 0
    RemoveEmptyLinesAfterAPIStart = 1
    SearchEndAPI = 2
    DoneAPI = 3

docs_folder = "docs/appendix/"
appendix_file = docs_folder + "apis.md"
btstack_folder = "../../../"
code_identation = "    "

api_header = """

## API_TITLE API
<a name ="appendix:API_LABLE"></a>

"""

api_ending = """
"""

# [file_name, api_title, api_lable]
list_of_apis = [ 
    [btstack_folder+"include/btstack/run_loop.h", "Run Loop", "api_run_loop"],
    [btstack_folder+"src/hci.h", "HCI", "api_hci"],
    [btstack_folder+"src/l2cap.h", "L2CAP", "api_l2cap"],
    [btstack_folder+"src/rfcomm.h", "RFCOMM", "api_rfcomm"],
    [btstack_folder+"src/sdp.h", "SDP", "api_sdp"],
    [btstack_folder+"src/sdp_client.h", "SDP Client", "api_sdp_client"],
    [btstack_folder+"src/sdp_query_rfcomm.h", "SDP RFCOMM Query", "api_sdp_queries"],
    [btstack_folder+"ble/gatt_client.h", "GATT Client", "api_gatt_client"],
    [btstack_folder+"src/pan.h", "PAN", "api_pan"],
    [btstack_folder+"src/bnep.h", "BNEP", "api_bnep"],
    [btstack_folder+"src/gap.h", "GAP", "api_gap"],
    [btstack_folder+"ble/sm.h", "SM", "api_sm"]
]

def replacePlaceholder(template, title, lable):
    api_title = title + " API"
    snippet = template.replace("API_TITLE", title).replace("API_LABLE", lable)
    return snippet

def writeAPI(fout, infile_name):
    global code_identation
    state = State.SearchStartAPI
    with open(infile_name, 'rb') as fin:
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
                fout.write(code_identation + line)

def process_and_write_api(fout, api_tuple):
    infile_name = api_tuple[0]
    if not infile_name:
        return
    
    api_title = api_tuple[1]
    api_lable = api_tuple[2]

    fout.write(replacePlaceholder(api_header, api_title, api_lable))
    writeAPI(fout, infile_name)


with open(appendix_file, 'w') as aout:
    for api_tuple in list_of_apis:
        infile_name = api_tuple[0]
        if not infile_name:
            continue
        process_and_write_api(aout, api_tuple)
        
