#!/usr/bin/env python

import sys, yaml
import os, re

figures = {
    'btstack-architecture'     : '1',
    'singlethreading-btstack'  : '0.3',
    'multithreading-monolithic': '0.8',
    'multithreading-btdaemon'  : '0.8',
    'btstack-protocols'        : '0.8'
}


def fix_empty_href(line):
    corr = re.match('.*(href{}).*',line)
    if corr:
        line = line.replace(corr.group(1), "path")
    return line


def fix_listing_after_section(line):
    corr = re.match('.*begin{lstlisting}',line)
    if corr:
        line = "\leavevmode" + line
    return line

def fix_listing_hyperref_into_ref(line):
    corr = re.match('(.*\\\\)hyperref\[(lst:.*)\]{.*}(.*)',line)
    if corr:
        line = corr.group(1)+"ref{" + corr.group(2) +"} " + corr.group(3) 
    return line


def fix_figure_width_and_type(line):
    global figures
    for name, width in figures.items():
        corr = re.match('(.*includegraphics)(.*'+name+'.*)',line)
        if corr:
            line = corr.group(1) + '[width='+width+'\\textwidth]' + corr.group(2).replace('png','pdf')
    return line


def fix_appendix_pagebreak(line):
    corr = re.match('.*section{APIs}.*',line)
    if corr:
        line = "\leavevmode\pagebreak\n" + line
    return line


def main(argv):
    docs_folder = "docs"
    yml_file = "mkdocs.yml"
    mk_file  = "latex/btstack_generated.md"

    with open(mk_file, 'w') as aout:
        with open(yml_file, 'r') as yin:
            doc = yaml.load(yin)
            for page in doc["pages"]:
                md_file = page[0]
                title = page[1]
                with open(docs_folder +"/"+ md_file, 'r') as mdin:
                    aout.write("\n\n#"+ title +"\n\n")
                    for line in mdin:
                        # remove path from section reference
                        # e.g. [the SPP Counter example](examples/generated/#sec:sppcounterExample)
                        # replace with [the SPP Counter example](#sec:sppcounterExample)
                        section_ref = re.match('.*\(((.*)(#sec:.*))\).*',line)
                        if section_ref:
                            line = line.replace(section_ref.group(2),"")
                        aout.write(line)               

    pandoc_cmd = "pandoc -f markdown -t latex --filter pandoc-fignos --filter pandoc-tablenos --listings latex/btstack_generated.md -o latex/btstack_generated.tex"
    p = os.popen(pandoc_cmd,"r")
    while 1:
        line = p.readline()
        if not line: break
        print line


    # btstatck_root_file = "latex/btstack_gettingstarted.tex"
    btstack_generated_file = "latex/btstack_generated.tex"
    btstack_final_file = "latex/btstack_final.tex"

    with open(btstack_final_file, 'w') as aout:
        aout.write("% !TEX root = btstack_gettingstarted.tex\n\n")

        with open(btstack_generated_file, 'r') as fin:
            for line in fin:
                line = fix_empty_href(line)
                line = fix_listing_after_section(line)
                line = fix_listing_hyperref_into_ref(line)
                line = fix_figure_width_and_type(line)
                line = fix_appendix_pagebreak(line)
                aout.write(line)       

             
if __name__ == "__main__":
   main(sys.argv[1:])
