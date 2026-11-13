#!/usr/bin/env python3

import sys, yaml
import re, getopt, subprocess

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
        line = r"\leavevmode" + line
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
        line = r"\leavevmode\pagebreak\n" + line
    return line

def fix_tightlist(line):
    if 'tightlist' in line:
        return ''
    else:
        return line

def postprocess_file(markdown_filepath, fout, title, pdf_html=False):
    with open(markdown_filepath, 'r') as fin:
        for line in fin:
            if pdf_html:
                if line.strip() == '<!-- pdf-config-table -->':
                    fout.write('::: {.pdf-config-table}\n')
                    continue
                if line.strip() == '<!-- /pdf-config-table -->':
                    fout.write(':::\n')
                    continue
            if line == "#\n":
                fout.write("\n\n# "+ title +"\n\n")
                continue
            # remove path from section reference
            # e.g. [the SPP Counter example](examples/generated/#sec:sppcounterExample)
            # replace with [the SPP Counter example](#sec:sppcounterExample)
            line = re.sub(r'\([^()\s#]*?(#sec:[^()\s]+)\)', r'(\1)', line)
            fout.write(line)
def main(argv):
    yml_file = "mkdocs.yml"
    latexfolder = "latex/"
    mkdocsfolder = "docs/"
    markdown_only = False
    
    cmd = 'markdown2tex.py [-i <mkdocsfolder>] [-o <latexfolder>] [--markdown-only]'

    try:
        opts, args = getopt.getopt(argv,"i:o:",["ifolder=","ofolder=", "markdown-only"])
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
        elif opt == "--markdown-only":
            markdown_only = True

    latex_filepath = latexfolder + "btstack_generated.md"
    
    with open(latex_filepath, 'wt') as fout:
        # The WeasyPrint PDF renders code blocks at 12 pt. 
        # Reduce them without requiring a separate stylesheet.
        fout.write("<style>pre > code { font-size: 9pt; } .code-operator { white-space: nowrap; } "
                   ".pdf-config-table table { width: 100%; table-layout: fixed; } "
                   ".pdf-config-table th:first-child, .pdf-config-table td:first-child { width: 42%; } "
                   ".pdf-config-table td:first-child { font-family: monospace; font-size: 0.85em; } "
                   ".pdf-config-table th, .pdf-config-table td { overflow-wrap: anywhere; }</style>\n\n")
        with open(yml_file, 'rt') as yin:
            doc = yaml.load(yin, Loader=yaml.SafeLoader)
            for page in doc["nav"]:
                navigation_group_filepath = list(page.values())[0]
                navigation_group_title = list(page.keys())[0]
                markdown_filepath = mkdocsfolder + navigation_group_filepath
                postprocess_file(markdown_filepath, fout, navigation_group_title, pdf_html=markdown_only)

    if markdown_only:
        return

    # btstatck_root_file = "latex/btstack_gettingstarted.tex"
    btstack_generated_file = latexfolder + "btstack_generated.tex"
    btstack_final_file = latexfolder + "btstack_final.tex"

    subprocess.run(['pandoc', '-f', 'markdown', '-t', 'latex', '--wrap=auto',
                    '--columns=34', '--listings', latex_filepath, '-o', btstack_generated_file], check=True)

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
