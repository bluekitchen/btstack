#!/usr/bin/env python
import os
import re
import sys 

class State:
    SearchStartAPI = 0
    RemoveEmptyLinesAfterAPIStart = 1
    SearchEndAPI = 2
    DoneAPI = 3

docs_folder = ""
appendix_file = docs_folder + "appendix_apis.tex"

api_header = """% !TEX root = btstack_gettingstarted.tex
\section{API_TITLE}
\label{appendix:API_LABLE}
$ $
\\begin{lstlisting}
"""

api_ending = """\end{lstlisting}
\pagebreak
"""

# [file_name, api_title, api_lable]
list_of_apis = [ 
    ["../../include/btstack/run_loop.h", "Run Loop", "api_run_loop"],
    ["../../src/hci.h", "Host Controller Interface (HCI)", "api_hci"],
    ["../../src/l2cap.h", "L2CAP", "api_l2cap"],
    ["../../src/rfcomm.h", "RFCOMM", "api_rfcomm"],
    ["../../src/sdp.h", "SDP", "api_sdp"],
    ["../../src/sdp_client.h", "SDP Client", "api_sdp_client"],
    ["../../src/sdp_query_rfcomm.h", "SDP RFCOMM Query", "api_sdp_queries"],
    ["../../ble/gatt_client.h", "GATT Client", "api_gatt_client"]
]

def replacePlaceholder(template, title, lable):
    api_title = title + " API"
    snippet = template.replace("API_TITLE", title).replace("API_LABLE", lable)
    return snippet

def writeAPI(fout, infile_name):
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
                fout.write(line)

for api_tuple in list_of_apis:
    infile_name = api_tuple[0]
    if not infile_name:
        continue
    
    api_title = api_tuple[1]
    api_lable = api_tuple[2]
    outfile_name = docs_folder + api_lable + ".tex"
    
    with open(outfile_name, 'w') as fout:
        fout.write(replacePlaceholder(api_header, api_title, api_lable))
        writeAPI(fout, infile_name)
        fout.write(api_ending)


with open(appendix_file, 'w') as aout:
    aout.write("% !TEX root = btstack_gettingstarted.tex\n\n")
    for api_tuple in list_of_apis:
        infile_name = api_tuple[0]
        if not infile_name:
            continue
        tex_input = "\input{" + api_tuple[2]+ "}\n"
        aout.write(tex_input)
