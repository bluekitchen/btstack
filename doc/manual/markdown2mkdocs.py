#!/usr/bin/env python

import sys, os, shutil
import re, yaml

# helper to write anchors and references
def insert_anchor(mdout, reference):
    anchor = "<a name=\"" + reference + "\"></a>\n\n"
    mdout.write(anchor)

def insert_reference(mdout, text, link):
    mdout.write("")

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

def main(argv):
    md_template = "docs"
    md_final = "docs_final"
    yml_file = "mkdocs.yml"
    
    with open(yml_file, 'r') as yin:
        doc = yaml.load(yin)
        for page in doc["pages"]:
            mk_file = page.values()[0]
            source_file = md_template +"/"+ mk_file
            dest_file   = md_final +"/"+ mk_file
            print("Processing %s -> %s" % (source_file, dest_file))
            with open(dest_file, 'w') as mdout:
                with open(source_file, 'r') as mdin:
                    for line in mdin:
                        line = process_section(mdin, mdout, line)
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

if __name__ == "__main__":
   main(sys.argv[1:])
