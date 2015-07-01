#!/usr/bin/env python
import os, sys, getopt, re, pickle

class State:
    SearchStartAPI = 0
    SearchEndAPI = 1
    DoneAPI = 2

# [file_name, api_title, api_lable]
apis = [ 
    ["src/bnep.h", "BNEP", "bnep"],
    ["src/btstack_memory.h","Memory Management","btMemory"],
    ["src/gap.h", "GAP", "gap"],
    ["ble/gatt_client.h", "GATT Client", "gattClient"],
    ["src/hci.h", "HCI", "hci"],
    ["src/hci_dump.h","Logging","hciTrace"],
    ["src/hci_transport.h","HCI Transport","hciTransport"],
    ["src/l2cap.h", "L2CAP", "l2cap"],
    ["src/pan.h", "PAN", "pan"],
    ["src/remote_device_db.h","Remote Device DB","rdevDb"],
    ["src/rfcomm.h", "RFCOMM", "rfcomm"],
    ["include/btstack/run_loop.h", "Run Loop", "runLoop"],
    ["ble/sm.h", "SM", "sm"],
    ["src/sdp.h", "SDP", "sdp"],
    ["src/sdp_client.h", "SDP Client", "sdpClient"],
    ["src/sdp_parser.h","SDP Parser","sdpParser"],
    ["src/sdp_query_rfcomm.h", "SDP RFCOMM Query", "sdpQueries"],
    ["src/sdp_query_util.h","SDP Query Util","sdpQueryUtil"],
    ["include/btstack/sdp_util.h","SDP Utils", "sdpUtil"]
]

functions = {}
typedefs = {}

api_header = """

## API_TITLE API {#sec:API_LABLEAPIAppendix}

"""

api_ending = """
"""

code_ref = """GITHUBFPATH#LLINENR"""


def codeReference(fname, githubfolder, filepath, linenr):
    global code_ref
    ref = code_ref.replace("GITHUB", githubfolder)
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
                        parts = re.match('.*API_START.*',line)
                        if parts:
                            state = State.SearchEndAPI
                        continue
                    
                    if state == State.SearchEndAPI:
                        parts = re.match('.*API_END.*',line)
                        if parts:
                            state = State.DoneAPI
                            continue
                        fout.write(mk_codeidentation + line)
                        continue


def createIndex(btstackfolder, apis, githubfolder):
    global typedefs, functions

    for api_tuple in apis:
        api_filename = btstackfolder + api_tuple[0]
        api_title = api_tuple[1]
        api_lable = api_tuple[2]

        linenr = 0
        with open(api_filename, 'rb') as fin:
            typedefFound = 0

            for line in fin:
                linenr = linenr + 1
                
                # search typedef struct begin
                typedef =  re.match('.*typedef\s+struct.*', line)
                if typedef:
                    typedefFound = 1
                
                # search typedef struct begin
                if typedefFound:
                    typedef = re.match('}\s*(.*);\n', line)
                    if typedef:
                        typedefFound = 0
                        typedefs[typedef.group(1)] = codeReference(typedef.group(1), githubfolder, api_tuple[0], linenr)
                    continue

                function =  re.match('.*typedef\s+void\s+\(\s*\*\s*(.*?)\)\(.*', line)
                if function:
                    functions[function.group(1)] = codeReference(function.group(1), githubfolder, api_tuple[0], linenr)
                    continue

                function = re.match('.*?\s+\*?\s*(.*?)\(.*\(*.*;', line)
                if function:
                    functions[function.group(1)] = codeReference(function.group(1), githubfolder, api_tuple[0], linenr)
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
    createIndex(btstackfolder, apis, githubfolder)

    references = functions.copy()
    references.update(typedefs)

    with open(indexfile, 'w') as fout:
        for function, reference in references.items():
            fout.write("[" + function + "](" + reference + ")\n")


    pickle.dump(references, open( "tmp/references.p", "wb" ) )


if __name__ == "__main__":
   main(sys.argv[1:])
