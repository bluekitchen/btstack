#!/usr/bin/env python3

import sys, os, shutil, getopt
import re, yaml
import subprocess

githuburl = "https://github.com/bluekitchen/btstack/tree/"
gitbranchname = "master"

# helper to write anchors and references
def insert_anchor(mdout, reference):
    anchor = "<a name=\"" + reference + "\"></a>\n\n"
    mdout.write(anchor)

def insert_reference(mdout, text, link):
    mdout.write("")

def process_source_file_link(mdin, mdout, githuburl, line):
    parts = re.match('.*(GITHUB_URL).*\n',line)
    if parts:
        line_with_source_file_link = line.replace("GITHUB_URL", githuburl)
        mdout.write(line_with_source_file_link)
        line = ''
    return line

# handlers for various elements
def process_section(mdin, mdout, line):
    section = re.match('(#+.*){#(sec:.*)}',line)
    if section:
        insert_anchor(mdout, section.group(2))
        mdout.write(section.group(1)+"\n")
        line = ''
    return line

def process_figure(mdin, mdout, line):
    # detect figure
    figure = re.match('\s*(\!.*)({#(fig:.*)})',line)
    if figure:
        insert_anchor(mdout, figure.group(3))
        mdout.write(figure.group(1)+"\n")
        line = ''
    return line

def process_fig_ref(mdin, mdout, line):
    # detect figure reference
    figure_ref = re.match('.*({@(fig:.*)})',line)
    if figure_ref:
        md_reference = "[below](#"+figure_ref.group(2)+")"
        line = line.replace(figure_ref.group(1), md_reference) 
        mdout.write(line)
        line = ''
    return line

def process_table(mdin, mdout, line):
    # detect table
    table = re.match('\s*(Table:.*)({#(tbl:.*)})',line)
    if table:
        insert_anchor(mdout, table.group(3))
        mdout.write(table.group(1)+"\n")
        line = ''
    return line

def process_tbl_ref(mdin, mdout, line):
    table_ref = re.match('.*({@(tbl:.*)})',line)
    if table_ref:
        md_reference = "[below](#"+table_ref.group(2)+")"
        line = line.replace(table_ref.group(1), md_reference) 
        mdout.write(line)
        line = ''
    return line

def process_listing(mdin, mdout, line):
    listing_start = re.match('.*{#(lst:.*)\s+.c\s+.*',line)
    listing_end   = re.match('\s*~~~~\s*\n',line)
    if listing_start:
        insert_anchor(mdout, listing_start.group(1))
        line = ''
    elif listing_end:
        mdout.write("\n")
        line = ''
    return line

def process_file(mk_file, markdownfolder, mkdocsfolder, githuburl):
    source_file = markdownfolder +"/"+ mk_file
    dest_file   = mkdocsfolder +"/"+ mk_file
    # print("Processing %s -> %s" % (source_file, dest_file))

    with open(dest_file, 'wt') as mdout:
        with open(source_file, 'rt') as mdin:
            for line in mdin:
                line = process_section(mdin, mdout, line)
                if len(line) == 0:
                    continue
                line = process_source_file_link(mdin, mdout, githuburl, line)
                if len(line) == 0:
                    continue
                line = process_figure(mdin, mdout, line)
                if len(line) == 0:
                    continue
                line = process_fig_ref(mdin, mdout, line)
                if len(line) == 0:
                    continue
                line = process_table(mdin, mdout, line)
                if len(line) == 0:
                    continue
                line = process_tbl_ref(mdin, mdout, line)
                if len(line) == 0:
                    continue
                line = process_listing(mdin, mdout, line)
                if len(line) == 0:
                    continue
                mdout.write(line)

def main(argv):
    markdownfolder = "docs-markdown/"
    mkdocsfolder = "docs/"
    
    cmd = 'markdown_update_references.py [-i <markdownfolder>] [-o <mkdocsfolder>] [-g <githuburl>]'

    try:
        opts, args = getopt.getopt(argv,"i:o:g:",["ifolder=","ofolder=","github="])
    except getopt.GetoptError:
        print (cmd)
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print (cmd)
            sys.exit()
        elif opt in ("-i", "--ifolder"):
            markdownfolder = arg
        elif opt in ("-o", "--ofolder"):
            mkdocsfolder = arg
        elif opt in ("-g", "--github"):
            githuburl = arg

    try:
        output = subprocess.check_output("git symbolic-ref --short HEAD", stderr=subprocess.STDOUT, timeout=3, shell=True)
        gitbranchname = output.decode().rstrip()
    except subprocess.CalledProcessError as exc:
        print('GIT branch name: failed to get, use default value \"%s\""  ', gitbranchname, exc.returncode, exc.output)
    else:
        print('GIT branch name:  %s' % gitbranchname)
        
    githuburl = githuburl + gitbranchname
    print('GITHUB URL:       %s\n' % githuburl)

    yml_file = "mkdocs.yml"
    
    with open(yml_file, 'r') as yin:
        doc = yaml.load(yin, Loader=yaml.SafeLoader)
        
        # page is either:
        # - {title: filepath} dictionary for a direcr yml reference (e.g.  - 'Welcome': index.md), or
        # - {navigation_group_title: [{title: filepath}, ...] } dictionary for a navigation group 
        for page in doc["nav"]:    
            
            # navigation_group_filepath is either:
            # - filepath string for a direcr yml reference (e.g.  - 'Welcome': index.md), or
            # - list of [{title: filepath}, ...] dictionaries for each item in navigation group 
            navigation_group_filepath = list(page.values())[0]

            if type(navigation_group_filepath) == str:
                process_file(navigation_group_filepath, markdownfolder, mkdocsfolder, githuburl)
                continue

            if type(navigation_group_filepath) == list:
                for file_description_dict in navigation_group_filepath:
                    filepath = list(file_description_dict.values())[0]
                    process_file(filepath, markdownfolder, mkdocsfolder, githuburl)
                continue
            
                
if __name__ == "__main__":
   main(sys.argv[1:])
