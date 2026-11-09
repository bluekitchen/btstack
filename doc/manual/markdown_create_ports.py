#!/usr/bin/env python3

import sys, os, shutil, re,  getopt
import io
from markdown_navigation import update_navigation

blacklist = []

port_item = """
- [PORT_TITLE](#sec:PORT_LABELPort)"""

def get_readme_title(example_path):
    title = ''
    with open(example_path, 'r') as fin:
        for line in fin:
            parts = re.match(r'(##\s)(.*)\n',line)
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
                    if file.endswith('.jpg') or file.endswith('.png'):
                        images[file] =  port_folder + "/" + port + "/" + file

    with open(ports_file, 'w') as ports:
        with open(intro_file, 'r') as fin:
            for line in fin:
                ports.write(line)
        fin.close()

        # Sort by directory name, but keep archived ports last in both the
        # index and the documentation.
        ordered_readmes = sorted(
            matches.items(),
            key=lambda item: (item[0] == "archive", item[0])
        )

        for readme_dir, readme_file in ordered_readmes:
            with open(readme_file, 'r') as fin:
                for line in fin:
                    # find title, add reference
                    title_parts = re.match(r'(#\s+)(.*)\n',line)
                    if title_parts:
                        title = title_parts.group(2)
                        ports.write(port_item.replace("PORT_TITLE", title).replace("PORT_LABEL", readme_dir))
                        break
        fin.close()
        ports.write("\n\n")

        ports.flush()
        with open(ports_file) as index_file:
            index_text = index_file.read()
        nav_entries = []
        for readme_dir, readme_file in ordered_readmes:
            page = io.StringIO()
            found_port_title = False
            with open(readme_file, 'r') as fin:
                for line in fin:
                    heading = re.match(r'(#+)\s+(.*)\n', line)
                    if heading:
                        level = len(heading.group(1))
                        title = heading.group(2)

                        # Separate concatenated READMEs so Pandoc cannot treat
                        # a port title as continuation text of the prior port.
                        # Only the README title receives the stable port anchor.
                        if level == 1 and not found_port_title:
                            page.write("\n## " + title + " {#sec:" + readme_dir + "Port}\n\n")
                            nav_entries.append((title, "ports/" + readme_dir + ".md"))
                            found_port_title = True
                        else:
                            page.write("\n" + "#" * (level + 1) + " " + title + "\n\n")
                    else:
                        page.write(line)
            content = page.getvalue()
            ports.write(content)
            with open(os.path.join(ports_folder, readme_dir + ".md"), "w") as output:
                output.write(content)
            index_text = index_text.replace("(#sec:" + readme_dir + "Port)", "(" + readme_dir + ".md#sec:" + readme_dir + "Port)")

        with open(os.path.join(ports_folder, "existing_ports.md"), "w") as index_file:
            index_file.write(index_text)
        update_navigation("Existing Ports", "ports/existing_ports.md", "ports/all.md", nav_entries)

        # copy images
        for image_filename, image_path in images.items():
            print('copy %s as %s' % (image_path, ports_folder + image_filename))
            shutil.copy(image_path, ports_folder + image_filename)

        fin.close()
        ports.close()

def main(argv):
    btstackfolder = "../../"
    markdownfolder = "docs-markdown/"
    templatefolder    = "docs-intro/"
    
    cmd = 'markdown_create_ports.py [-r <root_btstackfolder>] [-t <templatefolder>] [-o <output_markdownfolder>]'
    
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


    inputfolder = btstackfolder   + "port/"
    portsfolder = markdownfolder      + "ports/"
    introfile   = templatefolder + "ports_intro.md"
    outputfile  = portsfolder     + "all.md"

    print ('Input folder: ', inputfolder)
    print ('Intro file:   ', introfile)
    print ('Output file:  ', outputfile)

    process_readmes(introfile, inputfolder, outputfile, portsfolder)

if __name__ == "__main__":
   main(sys.argv[1:])
