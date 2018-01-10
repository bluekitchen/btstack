#!/usr/bin/env python

import sys, os, shutil, re

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
def process_readmes(intro_file, port_folder, ports_file, ports_folder):
    matches = {}
    images  = {}

    # iterate over port folders
    ports = os.listdir(port_folder)
    for port in ports:
        if port not in blacklist:
            readme_file = port_folder + "/" + port + "/" + "README.md"
            if os.path.exists(readme_file):
                matches[port] = readme_file
                for file in os.listdir(port_folder + "/" + port):
                    if file.endswith('.jpg'):
                        images[file] =  port_folder + "/" + port + "/" + file

    with open(ports_file, 'w') as ports:
        with open(intro_file, 'rb') as fin:
            for line in fin:
                ports.write(line)
        fin.close()

        for readme_dir, readme_file in sorted(matches.items()):
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

        for readme_dir, readme_file in sorted(matches.items()):
            with open(readme_file, 'rb') as fin:
                for line in fin:
                    #increase level of indentation
                    parts = re.match('#(.*)\n',line)
                    title_parts = re.match('(#\s+)(.*)\n',line)
                    if parts and title_parts:
                        ports.write("# " + title_parts.group(2) + " {" + "#sec:" + readme_dir + "Port}\n" )
                    else:
                        ports.write(line)

        # copy images
        for image_filename, image_path in images.items():
            print('copy %s as %s' % (image_path, ports_folder + image_filename))
            shutil.copy(image_path, ports_folder + image_filename)

        fin.close()
        ports.close()

def main(argv):
    btstackfolder = "../../"
    docsfolder    = "docs/"
    
    inputfolder = btstackfolder + "port/"
    portsfolder = docsfolder + "ports/"
    introfile   = portsfolder + "intro.md"
    outputfile  = portsfolder + "existing_ports.md"
    process_readmes(introfile, inputfolder, outputfile, portsfolder)

if __name__ == "__main__":
   main(sys.argv[1:])
