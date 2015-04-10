#!/usr/bin/env python
import os
import re
import sys 

docs_folder = ""
appendix_file = docs_folder + "examples.tex"

lst_header = """
\\begin{lstlisting}
"""

lst_ending = """
\end{lstlisting}
"""

example_header = """
\documentclass[11pt, oneside]{article}      
\usepackage{geometry}                       
\geometry{letterpaper}                         
%\geometry{landscape}                       
%\usepackage[parfill]{parskip}          
\usepackage{graphicx}                     
\usepackage{amssymb}
\usepackage{listings}
\usepackage{hyperref}
\\begin{document}
\section{Examples}
"""

example_ending = """
\end{document}  
"""

example_item = """
    \item \emph{EXAMPLE_TITLE}: EXAMPLE_DESC, see Section \\ref{example:EXAMPLE_LABLE}.
"""
example_section = """
\subsection{EXAMPLE_TITLE: EXAMPLE_DESC}
\label{example:EXAMPLE_LABLE}

"""
example_subsection = """
\subsubsection{LISTING_CAPTION}
"""

listing_start = """
\\begin{lstlisting}[float, caption= LISTING_CAPTION., label=LISTING_LABLE]
"""

listing_ending = """
\end{lstlisting}

"""
msp_folder = "../../platforms/msp-exp430f5438-cc2564b/example/"
# Example group title: [folder, example file, section title]
list_of_examples = { 
    "UART" : [[msp_folder, "led_counter", "provides UART and timer interrupt without Bluetooth"]]
    #"GAP"  : [["", "gap_inquiry"]],
    #"SPP Server" : [["", "spp_counter"],
    #                ["", "spp_accel"],
    #                ["", "spp_flowcontrol"]],
}

class State:
    SearchExampleStart = 0
    SearchSnippetStart = 2
    SearchSnippetPause = 4
    SearchSnippetResume = 5
    SearchSnippetEnd = 6
    ReachedExampleEnd = 7

def replacePlaceholder(template, title, lable):
    snippet = template.replace("API_TITLE", title).replace("API_LABLE", lable)
    return snippet


def writeListings(fout, infile_name):
    lst_lable = ""
    lst_caption = ""
    listing = ""
    state = State.SearchExampleStart
    with open(infile_name, 'rb') as fin:
        for line in fin:
            section_parts = re.match('.*(@section)\s*(.*)(\s*\*/)\n',line)
            if section_parts:
                aout.write(example_subsection.replace("LISTING_CAPTION", section_parts.group(2)))
                continue

            brief_parts = re.match('.*(@brief)\s*(.*)\n',line)
            if brief_parts:
                brief = brief_parts.group(2)+"\n"
                if lst_lable:
                    brief = brief.replace(lst_lable, "\\ref{"+lst_lable+"}")
                    lst_lable = ""
                aout.write(brief)
                if listing:
                    aout.write(listing)
                    listing = ""
                continue

            if state == State.SearchExampleStart:
                parts = re.match('.*(EXAMPLE_START)\((.*)\):\s*(.*)(\*/)?\n',line)
                if parts: 
                    lable = parts.group(2)
                    title = parts.group(2).replace("_","\_")
                    desc  = parts.group(3).replace("_","\_")
                    aout.write(example_section.replace("EXAMPLE_TITLE", title).replace("EXAMPLE_DESC", desc).replace("EXAMPLE_LABLE", lable))
                    state = State.SearchSnippetStart
                    continue

            if state == State.SearchSnippetStart:
                parts = re.match('.*(SNIPPET_START)\((.*)\):\s*(.*)(\*/)?\n',line)
                if parts: 
                    lst_lable = parts.group(2)
                    lst_caption = parts.group(3).replace("_","\_")
                    listing = listing_start.replace("LISTING_CAPTION", lst_caption).replace("LISTING_LABLE", lst_lable)
                    state = State.SearchSnippetEnd
                    continue
            
            if state == State.SearchSnippetEnd:
                parts_end = re.match('.*(SNIPPET_END).*',line)
                parts_pause = re.match('.*(SNIPPET_PAUSE).*',line)
                
                if not parts_end and not parts_pause:
                    end_comment_parts = re.match('.*(\*)/*\s*\n', line);
                    if not end_comment_parts:
                        aout.write(line)
                elif parts_end:
                    aout.write(listing_ending)
                    state = State.SearchSnippetStart
                elif parts_pause:
                    aout.write("...\n")
                    state = State.SearchSnippetResume
                continue
                
            if state == State.SearchSnippetResume:
                parts = re.match('.*(SNIPPET_RESUME).*',line)
                if parts:
                    state = State.SearchSnippetEnd
                    continue
        
            parts = re.match('.*(EXAMPLE_END).*',line)
            if parts:
                if state != State.SearchSnippetStart:
                    print "Formating error detected"
                state = State.ReachedExampleEnd
                print "Reached end of the example"
            
    

# write list of examples
with open(appendix_file, 'w') as aout:
    aout.write(example_header)
    aout.write("\\begin{itemize}\n");
    
    for group_title, examples in list_of_examples.iteritems():
        group_title = group_title + " example"
        if len(examples) > 1:
            group_title = group_title + "s"
        group_title = group_title + ":"

        aout.write("  \item " + group_title + "\n");
        aout.write("  \\begin{itemize}\n");
        for example in examples:
            lable = example[1]
            title = example[1].replace("_","\_")
            desc =  example[2].replace("_","\_")
            aout.write(example_item.replace("EXAMPLE_TITLE", title).replace("EXAMPLE_DESC", desc).replace("EXAMPLE_LABLE", lable))
        aout.write("  \\end{itemize}\n")
    aout.write("\\end{itemize}\n")

    for group_title, examples in list_of_examples.iteritems():
        for example in examples:
            file_name = example[0] + example[1] + ".c"
            writeListings(aout, file_name)

    aout.write(example_ending)

