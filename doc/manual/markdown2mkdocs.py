#!/usr/bin/env python

import sys, os, shutil
import re, yaml

def insert_anchor(mdout, reference):
    mdout.write("<a name=\"" + reference + "\"></a>\n\n")

def insert_reference(mdout, text, link):
    mdout.write("")

def process_sections(temp_file, dest_file):
    with open(dest_file, 'w') as mdout:
        with open(temp_file, 'r') as mdin:
            for line in mdin:
                section = re.match('(#+.*){#(sec:.*)}',line)
                if section:
                    insert_anchor(mdout, section.group(2))
                    mdout.write(section.group(1)+"\n")
                else:
                    mdout.write(line)
                
    shutil.copyfile(dest_file, temp_file)
    return

def process_figures(temp_file, dest_file):
    with open(dest_file, 'w') as mdout:
        with open(temp_file, 'r') as mdin:
            for line in mdin:
                # detect figure
                figure = re.match('\s*(\!.*)({#(fig:.*)})',line)
                if figure:
                    insert_anchor(mdout, figure.group(3))
                    mdout.write(figure.group(1)+"\n")
                else:
                    figure_ref = re.match('.*({@(fig:.*)})',line)
                    if figure_ref:
                        md_reference = "[below](#"+figure_ref.group(2)+")"
                        line = line.replace(figure_ref.group(1), md_reference) 
                    mdout.write(line)
    shutil.copyfile(dest_file, temp_file)
    return

def process_tables(temp_file, dest_file):
    with open(dest_file, 'w') as mdout:
        with open(temp_file, 'r') as mdin:
            for line in mdin:
                # detect table
                table = re.match('\s*(Table:.*)({#(tbl:.*)})',line)
                if table:
                    insert_anchor(mdout, table.group(3))
                    mdout.write(table.group(1)+"\n")
                else:
                    table_ref = re.match('.*({@(tbl:.*)})',line)
                    if table_ref:
                        md_reference = "[below](#"+table_ref.group(2)+")"
                        line = line.replace(table_ref.group(1), md_reference) 
                    mdout.write(line)
    shutil.copyfile(dest_file, temp_file)
    return


def process_listings(temp_file, dest_file):
    with open(dest_file, 'w') as mdout:
        with open(temp_file, 'r') as mdin:
            for line in mdin:
                listing_start = re.match('.*{#(lst:.*)\s+.c\s+.*',line)
                listing_end = re.match('\s*~~~~\s*\n',line)
                if listing_start:
                    insert_anchor(mdout, listing_start.group(1))
                elif listing_end:
                    mdout.write("\n")
                else:
                    mdout.write(line)
    shutil.copyfile(dest_file, temp_file)
    return



def main(argv):
    md_template = "docs"
    md_temp = "docs_tmp"
    md_final = "docs_final"
    yml_file = "mkdocs.yml"
    
    with open(yml_file, 'r') as yin:
        doc = yaml.load(yin)
        for page in doc["pages"]:
            source_file = md_template +"/"+ page[0]
            temp_file   = md_temp +"/"+ page[0]
            dest_file   = md_final +"/"+ page[0]
            
            process_sections(temp_file, dest_file)
            process_figures(temp_file, dest_file)
            process_tables(temp_file, dest_file)
            process_listings(temp_file, dest_file)


if __name__ == "__main__":
   main(sys.argv[1:])
