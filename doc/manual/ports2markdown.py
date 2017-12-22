#!/usr/bin/env python

import sys, os, shutil
import re, yaml
import fnmatch

blacklist = []

port_item = """
- [PORT_TITLE](#sec:PORT_LABELPort)"""

def get_readme_title(example_path):
    title = ''
    with open(example_path, 'rb') as fin:
        for line in fin:
            parts = re.match('(##\s)(.*)\n',line)
            if parts: 
                title = parts.group(2)
                continue
    return title

# write list of examples
def process_readmes(intro_file, ports_folder, ports_file):
    matches = {}
    
    for root, dirnames, filenames in os.walk(ports_folder):
        for filename in fnmatch.filter(filenames, 'README.md'):
            folder = os.path.basename(root)
            if folder not in blacklist:
                matches[folder] = os.path.join(root, filename)
        

    with open(ports_file, 'w') as ports:
        with open(intro_file, 'rb') as fin:
            for line in fin:
                ports.write(line)
        fin.close()

        for readme_dir, readme_file in matches.items():
            with open(readme_file, 'rb') as fin:
                for line in fin:
                    # find title, add reference
                    title_parts = re.match('(#\s+)(.*)\n',line)
                    if title_parts:
                        title = title_parts.group(2)
                        ports.write(port_item.replace("PORT_TITLE", title).replace("PORT_LABEL", readme_dir))
                        break
        fin.close()
        ports.write("\n\n")

        for readme_dir, readme_file in matches.items():
            with open(readme_file, 'rb') as fin:
                for line in fin:
                    #increase level of indetation
                    parts = re.match('#(.*)\n',line)
                    title_parts = re.match('(#\s+)(.*)\n',line)
                    if parts:
                        if title_parts:
                            ports.write("#" + title_parts.group(2) + " {" + "#sec:" + readme_dir + "Port}\n" )
                        else:
                            ports.write("#" + line)
                    else:
                        ports.write(line)


        fin.close()
        ports.close()

def main(argv):
    btstackfolder = "../../"
    docsfolder    = "docs/"
    
    inputfolder = btstackfolder + "port/"
    introfile   = docsfolder + "ports/intro.md"
    outputfile  = docsfolder + "ports/existing_ports.md"
    
    process_readmes(introfile, inputfolder, outputfile)

if __name__ == "__main__":
   main(sys.argv[1:])
