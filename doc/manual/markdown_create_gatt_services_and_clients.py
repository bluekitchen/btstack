#!/usr/bin/env python3
import os, sys, getopt, re
import subprocess

class State:
    SearchIntro = 0
    IntroFound = 1
    SearchAPI = 2

mdfiles = {
    # source file sufix : docu file, [white list od source files]
    "_server.h" : ["gatt_services.md", ["hids_device.h"]],
    "_client.h" : ["gatt_clients.md", []]
}

abrevations = {
    "Ancs" : "ANCS",
    "Hids" : "HIDS",
    "Ublox": "u-blox",
    "Spp"  : "SPP",
    "And"  : "and"
}

description_template = """
## NAME {#sec:REFERENCE}

"""

description_api = """

See [NAME API](../appendix/apis/#REFERENCE).

"""

def isEmptyCommentLine(line):
    return re.match(r'(\s*\*\s*)\n',line)

def isCommentLine(line):
    return re.match(r'(\s*\*\s*).*',line)

def isEndOfComment(line):
    return re.match(r'\s*\*/.*', line)

def isNewItem(line):
    return re.match(r'(\s*\*\s*\-\s*)(.*)',line)

def isTextTag(line):
    return re.match(r'.*(@text).*', line)

def isItemizeTag(line):
    return re.match(r'(\s+\*\s+)(-\s)(.*)', line)

def processTextLine(line):
    if isEmptyCommentLine(line):
        return "\n\n"

    line.rstrip()

    # add missing https://
    line = line.replace("developer.apple.com","https://developer.apple.com")

    if isTextTag(line):
        text_line_parts = re.match(r'.*(@text\s*)(.*)', line)
        return text_line_parts.group(2).lstrip() + " "

    if isItemizeTag(line):
        text_line_parts = re.match(r'(\s*\*\s*\-\s*)(.*)', line)
        return "- " + text_line_parts.group(2)
    
    if isEmptyCommentLine(line):
        return "\n"

    text_line_parts = re.match(r'(\s+\*\s+)(.*)', line)
    if text_line_parts:
        return text_line_parts.group(2) + " "
    return ""

def handle_abrevations(basename):
    name_parts = [item.capitalize() for item in basename.split("_")]
    for i in range(len(name_parts)):
        try:
            name_parts[i] = abrevations[name_parts[i]]
        except KeyError:
            continue
    
    return " ".join(name_parts)
    

def process_file(basename, inputfile_path, outputfile_path):
    reference = basename.replace("_", "-")
    name = handle_abrevations(basename)
    
    title = description_template.replace("NAME", name).replace("REFERENCE", reference)
    api  = description_api.replace("NAME", name).replace("REFERENCE", reference)
    
    text_block = ""
    with open(inputfile_path, 'r') as fin:
        state = State.SearchIntro
        for line in fin:
            if state == State.SearchIntro: 
                if isTextTag(line):
                    state = State.IntroFound
                    text_block = text_block + processTextLine(line)
                continue
    
            if state == State.IntroFound:
                text_block = text_block + processTextLine(line)
                
                if isEndOfComment(line):
                    state = State.SearchIntro
    fin.close()

    with open(outputfile_path, 'a+') as fout:
        fout.write(title)
        fout.write(text_block)
        fout.write(api)
    fout.close()
    
def main(argv):
    btstackfolder   = os.path.abspath(os.path.dirname(sys.argv[0]) + '/../../')
    inputfolder     = btstackfolder + "/src/ble/gatt-service/"
    
    markdownfolder = "docs-markdown/"
    templatefolder    = "docs-intro/"
    
    cmd = 'markdown_create_gatt_services_and_clients.py [-r <root_btstackfolder>] [-t <templatefolder>] [-o <output_markdownfolder>]'
    
    try:
        opts, args = getopt.getopt(argv,"r:t:o:",["rfolder=","tfolder=","ofolder="])
    except getopt.GetoptError:
        print (cmd)
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print (cmd)
            sys.exit()
        elif opt in ("-r", "--rfolder"):
            btstackfolder = arg
        elif opt in ("-t", "--tfolder"):
            templatefolder = arg
        elif opt in ("-o", "--ofolder"):
            markdownfolder = arg


    for source_filename_sufix, [outputfile, white_list] in mdfiles.items():
        outputfile_path = markdownfolder + outputfile 
        introfile_path = templatefolder + outputfile[:-3] + "_intro.md"
        
        with open(outputfile_path, 'w') as fout:
            with open(introfile_path, 'r') as fin:
                for line in fin:
                    fout.write(line)
        
        fin.close()   
        fout.close()

        files_to_process = []
        for root, dirs, files in os.walk(inputfolder, topdown=True):
            for f in files:
                if not f.endswith(source_filename_sufix): 
                    if f not in white_list:
                        continue
                files_to_process.append(root + "/"+ f)
        
        files_to_process.sort()

        for inputfile_path in files_to_process:
            basename = os.path.basename(inputfile_path)[:-2]
            process_file(basename, inputfile_path, outputfile_path)
        

if __name__ == "__main__":
   main(sys.argv[1:])
