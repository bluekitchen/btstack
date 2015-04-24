#!/usr/bin/env python
import os
import re
import sys, getopt

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

# Example group title: [folder, example file, section title]
list_of_examples = { 
    "Hello World" : [["led_counter", "UART and timer interrupt without Bluetooth"]],
    "GAP"         : [["gap_inquiry", "GAP Inquiry Example"]],
    "SDP Queries" :[["sdp_general_query", "SDP General Query"],
                    # ["sdp_bnep_query", "SDP BNEP Query"]
                    ],
    "SPP Server"  : [["spp_counter", "SPP Server - Heartbeat Counter over RFCOMM"],
                     ["spp_flowcontrol", "SPP Server - Flow Control"]],
    "BNEP/PAN"   : [["panu_demo", "PANU example"]],
    "Low Energy"  : [["gatt_browser", "GATT Client - Discovering primary services and their characteristics"],
                    ["le_counter", "LE Peripheral - Counter example"]],
    "Dual Mode " : [["spp_and_le_counter", "Dual mode example: Combined SPP Counter + LE Counter "]],
}

class State:
    SearchExampleStart = 0
    SearchListingStart = 1
    SearchListingPause = 2
    SearchListingResume = 3
    SearchListingEnd = 4
    SearchItemizeEnd = 5
    ReachedExampleEnd = 6

def replacePlaceholder(template, title, lable):
    snippet = template.replace("API_TITLE", title).replace("API_LABLE", lable)
    return snippet

def latexText(text):
    if not text:
        return ""

    brief = text.replace("_","\_")
    brief = brief.replace(" in the BTstack manual","")
    
    refs = re.match('.*(Listing\s*)(\w*).*',brief)
    if refs:
        brief = brief.replace(refs.group(2), "\\ref{listing:"+refs.group(2)+"}")
    refs = re.match('.*(Section\s*)(\w*).*',brief)
    if refs:
        brief = brief.replace(refs.group(2), "\\ref{section:"+refs.group(2)+"}")

    return brief

def isEmptyCommentLine(line):
    return re.match('(\s*\*\s*)\n',line)

def isEndOfComment(line):
    return re.match('\s*\*/.*', line)

def isNewItem(line):
    return re.match('(\s*\*\s*\-\s*)(.*)',line)

def isTextTag(line):
    return re.match('.*(@text).*', line)

def isItemizeTag(line):
    return re.match("(\s+\*\s+)(-\s)(.*)", line)

def processTextLine(line):
    if isTextTag(line):
        text_line_parts = re.match(".*(@text)(.*)", line)
        return " " + latexText(text_line_parts.group(2))

    if isItemizeTag(line):
        text_line_parts = re.match("(\s*\*\s*\-\s*)(.*)", line)
        return "\n \item " + latexText(text_line_parts.group(2))
    
    text_line_parts = re.match("(\s+\*\s+)(.*)", line)
    if text_line_parts:
        return " " + latexText(text_line_parts.group(2))
    return ""


def writeListings(aout, infile_name):
    itemText = None
    state = State.SearchExampleStart
    code_in_listing = ""
    text_block = None
    itemize_block = None

    with open(infile_name, 'rb') as fin:
        for line in fin:
            if state == State.SearchExampleStart:
                parts = re.match('.*(EXAMPLE_START)\((.*)\):\s*(.*)(\*/)?\n',line)
                if parts: 
                    lable = parts.group(2)
                    title = latexText(parts.group(2))
                    desc  = latexText(parts.group(3))
                    aout.write(example_section.replace("EXAMPLE_TITLE", title).replace("EXAMPLE_DESC", desc).replace("EXAMPLE_LABLE", lable))
                    state = State.SearchListingStart
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
            
            if isTextTag(line):
                text_block = processTextLine(line)
                continue

            if text_block:
                if isEndOfComment(line):
                    if itemize_block:
                        aout.write(itemize_block + "\n\end{itemize}\n")
                        itemize_block = None
                    else:
                        aout.write(text_block)
                        text_block = None
                else:
                    if isNewItem(line):
                        if not itemize_block:
                            itemize_block = "\n \\begin{itemize}"

                    if itemize_block:
                        itemize_block = itemize_block + processTextLine(line)
                    else:
                        text_block = text_block + processTextLine(line)

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
                print "Reached end of the example"
            

    
# write list of examples
def processExamples(examples_folder, standalone, examples_ofile):
    with open(examples_ofile, 'w') as aout:
        if standalone:
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
                lable = example[0]
                title = latexText(example[0])
                desc  = latexText(example[1])
                aout.write(example_item.replace("EXAMPLE_TITLE", title).replace("EXAMPLE_DESC", desc).replace("EXAMPLE_LABLE", lable))
            aout.write("  \\end{itemize}\n")
        aout.write("\\end{itemize}\n")

        for group_title, examples in list_of_examples.iteritems():
            for example in examples:
                file_name = examples_folder + example[0] + ".c"
                writeListings(aout, file_name)

        if standalone:
            aout.write(document_end)

def main(argv):
    inputfolder = "../../example/embedded/"
    outputfile = "examples.tex"
    standalone = 0

    try:
        opts, args = getopt.getopt(argv,"hiso:",["ifolder=","standalone","ofile="])
    except getopt.GetoptError:
        print 'update_listings.py [-i <inputfolder>] [-s] [-o <outputfile>]'
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print 'update_listings.py [-i <inputfolder>] [-s] [-o <outputfile>]'
            sys.exit()
        elif opt in ("-i", "--ifolder"):
            inputfolder = arg
        elif opt in ("-s", "--standalone"):
            standalone = 1
        elif opt in ("-o", "--ofile"):
            outputfile = arg
    print 'Input folder is ', inputfolder
    print 'Standalone flag is ', standalone
    print 'Output file is ', outputfile
    processExamples(inputfolder, standalone, outputfile)

if __name__ == "__main__":
   main(sys.argv[1:])
