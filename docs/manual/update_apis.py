#!/usr/bin/env python
import os, sys, getopt, re

class State:
    SearchStartAPI = 0
    RemoveEmptyLinesAfterAPIStart = 1
    SearchEndAPI = 2
    DoneAPI = 3

# [file_name, api_title, api_lable]
apis = [ 
    ["include/btstack/run_loop.h", "Run Loop", "runLoop"],
    ["src/hci.h", "HCI", "hci"],
    ["src/l2cap.h", "L2CAP", "l2cap"],
    ["src/rfcomm.h", "RFCOMM", "rfcomm"],
    ["src/sdp.h", "SDP", "sdp"],
    ["src/sdp_client.h", "SDP Client", "sdpClient"],
    ["src/sdp_query_rfcomm.h", "SDP RFCOMM Query", "sdpQueries"],
    ["ble/gatt_client.h", "GATT Client", "gattClient"],
    ["src/pan.h", "PAN", "pan"],
    ["src/bnep.h", "BNEP", "bnep"],
    ["src/gap.h", "GAP", "gap"],
    ["ble/sm.h", "SM", "sm"]
]

functions = {}
typedefs = {}

api_header = """

## API_TITLE API {#sec:API_LABLEAPIAppendix}

"""

api_ending = """
"""

code_ref = """[FNAME](GITHUBFPATH#LLINENR)"""

def codeReference(fname, githubfolder, filepath, linenr):
    global code_ref
    ref = code_ref.replace("FNAME",fname)
    ref = ref.replace("GITHUB", githubfolder)
    ref = ref.replace("FPATH", filepath)
    ref = ref.replace("LINENR", str(linenr))
    return ref


def writeAPI(apifile, btstackfolder, apis, mk_codeidentation):
    with open(apifile, 'w') as fout:
        for api_tuple in apis:
            api_filename = btstackfolder + api_tuple[0]
            api_title = api_tuple[1]
            api_lable = api_tuple[2]

            title = api_header.replace("API_TITLE", api_title).replace("API_LABLE", api_lable)
            fout.write(title)
            
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
                        continue

                    if state == State.SearchEndAPI:
                        parts = re.match('\s*(/\*).*API_END.*(\*/)',line)
                        if parts:
                            state = State.DoneAPI
                            return
                        fout.write(mk_codeidentation + line)
                        continue


def writeIndex(indexfile, btstackfolder, apis, githubfolder):
    global typedefs, functions

    with open(indexfile, 'w') as fout:
        for api_tuple in apis:
            api_filename = btstackfolder + api_tuple[0]
            api_title = api_tuple[1]
            api_lable = api_tuple[2]

            linenr = 0
            with open(api_filename, 'rb') as fin:
                typedefFound = 0

                for line in fin:
                    linenr = linenr + 1
                    
                    typedef =  re.match('.*typedef\s+struct.*', line)
                    if typedef:
                        typedefFound = 1
                        continue

                    if typedefFound:
                        typedef = re.match('}\s*(.*);\n', line)
                        if typedef:
                            typedefFound = 0
                            typedefs[typedef.group(1)] = codeReference(typedef.group(1), githubfolder, api_tuple[0], linenr)
                            fout.write(typedefs[typedef.group(1)]+"\n")
                        continue

                    function =  re.match('.*typedef\s+void\s+\(\s*\*\s*(.*?)\)\(.*', line)
                    if function:
                        functions[function.group(1)] = codeReference(function.group(1), githubfolder, api_tuple[0], linenr)
                        fout.write(functions[function.group(1)]+"\n")
                        continue

                    function = re.match('.*?\s+\*?\s*(.*?)\(.*\(*.*;', line)
                    if function:

                        functions[function.group(1)] = codeReference(function.group(1), githubfolder, api_tuple[0], linenr)
                        fout.write(functions[function.group(1)]+"\n")
                        continue

                        
def main(argv):
    mk_codeidentation = "    "
    
    btstackfolder = "../../"
    githubfolder  = "https://github.com/bluekitchen/btstack/blob/master/"
    
    docsfolder    = "docs/"
    apifile   = docsfolder + "appendix/apis.md"
    indexfile = docsfolder + "appendix/index.md"

    cmd = 'update_apis.py [-b <btstackfolder>] [-a <apifile>] [-g <githubfolder>] [-i <indexfile>]'
    try:
        opts, args = getopt.getopt(argv,"hiso:",["bfolder=","afile=","gfolder=","ifile="])
    except getopt.GetoptError:
        print cmd
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print cmd
            sys.exit()
        elif opt in ("-b", "--bfolder"):
            btstackfolder = arg
        elif opt in ("-a", "--afile"):
            apifile = arg
        elif opt in ("-g", "--gfolder"):
            btstackfolder = arg
        elif opt in ("-i", "--ifile"):
            indexfile = arg
    print 'BTstack folder is :', btstackfolder
    print 'API file is       :', apifile
    print 'Github path is    :', githubfolder
    print 'Index file is     :', indexfile

    writeAPI(apifile, btstackfolder, apis, mk_codeidentation)
    # writeIndex(indexfile, btstackfolder, apis, githubfolder)


if __name__ == "__main__":
   main(sys.argv[1:])
