#!/usr/bin/env python
import os
import re
import sys 

docs_folder = ""
appendix_file = docs_folder + "examples.tex"
stand_alone_doc = 0

lst_header = """
\\begin{lstlisting}
"""

lst_ending = """
\end{lstlisting}
"""

document_begin = """
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
"""

document_end = """
\end{document}  
"""

examples_header = """
% !TEX root = btstack_gettingstarted.tex
\section{Examples}
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
$ $
\\begin{lstlisting}[caption= LISTING_CAPTION., label=listing:LISTING_LABLE]
"""

listing_ending = """\end{lstlisting}

"""
msp_folder = "../../platforms/msp-exp430f5438-cc2564b/example/"
embedded_folder = "../../example/embedded/"
# Example group title: [folder, example file, section title]
list_of_examples = { 
    "UART" : [[embedded_folder, "led_counter", "UART and timer interrupt without Bluetooth"]],
    "GAP"  : [[embedded_folder, "gap_inquiry", "GAP Inquiry Example"]],
    "SPP Server" : [[embedded_folder, "spp_counter", "SPP Server - Heartbeat Counter over RFCOMM"],
                   [embedded_folder, "spp_flowcontrol", "SPP Server - Flow Control"]],
    "Low Energy" :[[embedded_folder, "gatt_browser", "GATT Client - Discovering primary services and their characteristics"],
                   [embedded_folder, "ble_peripheral", "LE Peripheral"]],
    "Dual Mode " :[[embedded_folder, "spp_and_le_counter", "Dual mode example"]],
    "SDP BNEP Query" :[[embedded_folder, "sdp_bnep_query", "SDP BNEP Query"]]
}

class State:
    SearchExampleStart = 0
    SearchBlockEnd = 1
    SearchListingStart = 2
    SearchListingPause = 4
    SearchListingResume = 5
    SearchListingEnd = 6
    SearchItemizeEnd = 7
    ReachedExampleEnd = 8

def replacePlaceholder(template, title, lable):
    snippet = template.replace("API_TITLE", title).replace("API_LABLE", lable)
    return snippet

def latexText(text):
    brief = text.replace("_","\_")
    brief = brief.replace(" in the BTstack manual","")
    
    refs = re.match('.*(Listing\s*)(\w*).*',brief)
    if refs:
        brief = brief.replace(refs.group(2), "\\ref{listing:"+refs.group(2)+"}")
    refs = re.match('.*(Section\s*)(\w*).*',brief)
    if refs:
        brief = brief.replace(refs.group(2), "\\ref{section:"+refs.group(2)+"}")

    return brief

def writeListings(fout, infile_name):
    itemText = None
    state = State.SearchExampleStart
    briefs_in_listings = ""
    code_in_listing = ""

    with open(infile_name, 'rb') as fin:
        for line in fin:
            if state == State.SearchExampleStart:
                parts = re.match('.*(EXAMPLE_START)\((.*)\):\s*(.*)(\*/)?\n',line)
                if parts: 
                    lable = parts.group(2)
                    title = latexText(parts.group(2))
                    desc  = latexText(parts.group(3))
                    aout.write(example_section.replace("EXAMPLE_TITLE", title).replace("EXAMPLE_DESC", desc).replace("EXAMPLE_LABLE", lable))
                    state = State.SearchBlockEnd
                continue
            if state == State.SearchBlockEnd:
                comment_end = re.match('(.*)\s(\*/)\n',line)
                comment_continue = re.match('(\s?\*\s+)(.*)',line)
                brief_start = re.match('.*(@text)\s*(.*)',line)
                
                if brief_start:
                    brief_part = "\n\n" + latexText(brief_start.group(2))
                    briefs_in_listings = briefs_in_listings + brief_part
                    state = State.SearchBlockEnd
                    continue
                
                if comment_end:
                    state = State.SearchListingStart
                elif comment_continue:
                    aout.write(latexText(comment_continue.group(2)))
                continue
            # detect @section
            section_parts = re.match('.*(@section)\s*(.*)\s*(\*?/?)\n',line)
            if section_parts:
                aout.write("\n" + example_subsection.replace("LISTING_CAPTION", section_parts.group(2)))
                continue

            # detect @subsection
            subsection_parts = re.match('.*(@subsection)\s*(.*)\s*(\*?/?)\n',line)
            if section_parts:
                subsubsection = example_subsection.replace("LISTING_CAPTION", section_parts.group(2)).replace('section', 'subsection')
                aout.write("\n" + subsubsection)
                continue
            # detect @text
            brief = None
            brief_start = re.match('.*(@text)\s*(.*)',line)
            if brief_start:
                brief_part = "\n\n" + latexText(brief_start.group(2))
                briefs_in_listings = briefs_in_listings + brief_part
                state = State.SearchBlockEnd
                continue

            # detect subsequent items
            if itemText:
                itemize_new = re.match('(\s*\*\s*\-\s*)(.*)',line)
                if itemize_new:
                    aout.write(itemText + "\n")
                    itemText = "\item "+ latexText(itemize_new.group(2))
                else:
                    empty_line = re.match('(\s*\*\s*)\n',line)
                    comment_end = re.match('\s*\*/.*', line)
                    if empty_line or comment_end:
                        aout.write(itemText + "\n")
                        aout.write("\n\end{itemize}\n")
                        itemText = None
                    else:
                        itemize_continuation = re.match('(\s*\*\s*)(.*)',line)
                        if itemize_continuation:
                            itemText = itemText + " " + latexText(itemize_continuation.group(2))
                continue
            else:
                # detect "-" itemize
                start_itemize = re.match('(\s*\*\s*-\s*)(.*)',line)
                if (start_itemize):
                    aout.write("\n \\begin{itemize}\n")
                    itemText = "\item "+ latexText(start_itemize.group(2))
                    continue

                brief_continue = re.match('(\s*\*\s)(.*)\s*\n',line)
                if brief_continue:
                    brief_part = " " + latexText(brief_continue.group(2))
                    briefs_in_listings = briefs_in_listings + brief_part
                    continue
            
            if state == State.SearchListingStart:
                parts = re.match('.*(LISTING_START)\((.*)\):\s*(.*\s*)(\*/).*',line)
                
                if parts: 
                    lst_lable = parts.group(2)
                    lst_caption = latexText(parts.group(3))
                    listing = listing_start.replace("LISTING_CAPTION", lst_caption).replace("LISTING_LABLE", lst_lable)
                    if listing:
                        aout.write("\n" + listing)
                    state = State.SearchListingEnd
                continue
            
            if state == State.SearchListingEnd:
                parts_end = re.match('.*(LISTING_END).*',line)
                parts_pause = re.match('.*(LISTING_PAUSE).*',line)
                end_comment_parts = re.match('.*(\*/)\s*\n', line);
                
                if parts_end:
                    aout.write(code_in_listing)
                    code_in_listing = ""
                    aout.write(listing_ending)
                    if briefs_in_listings:
                        aout.write(briefs_in_listings)
                        briefs_in_listings = ""
                    state = State.SearchListingStart
                elif parts_pause:
                    code_in_listing = code_in_listing + "...\n"
                    state = State.SearchListingResume
                elif not end_comment_parts:
                    # aout.write(line)
                    code_in_listing = code_in_listing + line.replace("    ", "  ")
                continue
                
            if state == State.SearchListingResume:
                parts = re.match('.*(LISTING_RESUME).*',line)
                if parts:
                    state = State.SearchListingEnd
                continue
        
            parts = re.match('.*(EXAMPLE_END).*',line)
            if parts:
                if state != State.SearchListingStart:
                    print "Formating error detected"
                state = State.ReachedExampleEnd
                if briefs_in_listings:
                    aout.write(briefs_in_listings)
                    briefs_in_listings = ""
                print "Reached end of the example"
            
    

# write list of examples
with open(appendix_file, 'w') as aout:
    if stand_alone_doc:
        aout.write(document_begin)
    aout.write(examples_header)
    aout.write("\n \\begin{itemize}\n");
    
    for group_title, examples in list_of_examples.iteritems():
        group_title = group_title + " example"
        if len(examples) > 1:
            group_title = group_title + "s"
        group_title = group_title + ":"

        aout.write("  \item " + group_title + "\n");
        aout.write("  \\begin{itemize}\n");
        for example in examples:
            lable = example[1]
            title = latexText(example[1])
            desc  = latexText(example[2])
            aout.write(example_item.replace("EXAMPLE_TITLE", title).replace("EXAMPLE_DESC", desc).replace("EXAMPLE_LABLE", lable))
        aout.write("  \\end{itemize}\n")
    aout.write("\\end{itemize}\n")

    for group_title, examples in list_of_examples.iteritems():
        for example in examples:
            file_name = example[0] + example[1] + ".c"
            writeListings(aout, file_name)

        if stand_alone_doc:
            aout.write(document_end)

