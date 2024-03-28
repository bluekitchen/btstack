#!/usr/bin/env python3

import sys, yaml
import os, re, getopt

pandoc_cmd_template = """
pandoc -f markdown -t latex --filter pandoc-fignos --filter pandoc-tablenos --listings LATEX_FOLDERbtstack_generated.md -o LATEX_FOLDERbtstack_generated.tex

"""

figures = {
    'btstack-architecture'     : '1',
    'singlethreading-btstack'  : '0.3',
    'multithreading-monolithic': '0.8',
    'multithreading-btdaemon'  : '0.8',
    'btstack-protocols'        : '0.8'
}


def fix_empty_href(line):
    corr = re.match(r'.*(href{}).*',line)
    if corr:
        line = line.replace(corr.group(1), "path")
    return line


def fix_listing_after_section(line):
    corr = re.match(r'.*begin{lstlisting}',line)
    if corr:
        line = "\leavevmode" + line
    return line

def fix_listing_hyperref_into_ref(line):
    corr = re.match(r'(.*\\)hyperref\[(lst:.*)\]{.*}(.*)',line)
    if corr:
        line = corr.group(1)+"ref{" + corr.group(2) +"} " + corr.group(3) 
    return line


def fix_figure_width_and_type(line):
    global figures
    for name, width in figures.items():
        corr = re.match(r'(.*includegraphics)(.*'+name+'.*)',line)
        if corr:
            line = corr.group(1) + '[width='+width+'\\textwidth]' + corr.group(2).replace('png','pdf')
    return line


def fix_appendix_pagebreak(line):
    corr = re.match(r'.*section{APIs}.*',line)
    if corr:
        line = "\leavevmode\pagebreak\n" + line
    return line

def fix_tightlist(line):
    if 'tightlist' in line:
        return ''
    else:
        return line

def postprocess_file(markdown_filepath, fout, title):
    with open(markdown_filepath, 'r') as fin:
        for line in fin:
            if line == "#\n":
                fout.write("\n\n#"+ title +"\n\n")
                continue
            # remove path from section reference
            # e.g. [the SPP Counter example](examples/generated/#sec:sppcounterExample)
            # replace with [the SPP Counter example](#sec:sppcounterExample)
            section_ref = re.match(r'.*\(((.*)(#sec:.*))\).*',line)
            if section_ref:
                line = line.replace(section_ref.group(2),"")
            fout.write(line)

def main(argv):
    yml_file = "mkdocs.yml"
    latexfolder = "latex/"
    mkdocsfolder = "docs/"
    
    cmd = 'markdown2tex.py [-i <mkdocsfolder>] [-o <latexfolder>] '

    try:
        opts, args = getopt.getopt(argv,"i:o:",["ifolder=","ofolder="])
    except getopt.GetoptError:
        print (cmd)
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print (cmd)
            sys.exit()
        elif opt in ("-i", "--ifolder"):
            mkdocsfolder = arg
        elif opt in ("-o", "--ofolder"):
            latexfolder = arg

    latex_filepath = latexfolder + "btstack_generated.md"
    
    with open(latex_filepath, 'wt') as fout:
        with open(yml_file, 'rt') as yin:
            doc = yaml.load(yin, Loader=yaml.SafeLoader)
            for page in doc["nav"]:
                navigation_group_filepath = list(page.values())[0]
                navigation_group_title = list(page.keys())[0]
                markdown_filepath = mkdocsfolder + navigation_group_filepath
                postprocess_file(markdown_filepath, fout, navigation_group_title)

    pandoc_cmd = pandoc_cmd_template.replace("LATEX_FOLDER", latexfolder)

    p = os.popen(pandoc_cmd,"r")
    while 1:
        line = p.readline()
        if not line: break
        print (line)


    # btstatck_root_file = "latex/btstack_gettingstarted.tex"
    btstack_generated_file = latexfolder + "btstack_generated.tex"
    btstack_final_file = latexfolder + "btstack_final.tex"

    with open(btstack_final_file, 'w') as aout:
        aout.write("% !TEX root = btstack_gettingstarted.tex\n\n")

        with open(btstack_generated_file, 'r') as fin:
            for line in fin:
                line = fix_empty_href(line)
                line = fix_listing_after_section(line)
                line = fix_listing_hyperref_into_ref(line)
                line = fix_figure_width_and_type(line)
                line = fix_appendix_pagebreak(line)
                line = fix_tightlist(line)
                aout.write(line)       

             
if __name__ == "__main__":
    main(sys.argv[1:])
