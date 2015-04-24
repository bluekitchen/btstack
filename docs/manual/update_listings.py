#!/usr/bin/env python
import os
import re
import sys, getopt

# Defines the names of example groups. Preserves the order in which the example groups will be parsed.
list_of_groups = ["Hello World", "GAP", "SDP Queries", "SPP Server", "BNEP/PAN", "Low Energy", "Dual Mode"]

# Defines which examples belong to a group. Example is defined as [example file, example title].
list_of_examples = { 
    "Hello World" : [["led_counter"]],
    "GAP"         : [["gap_inquiry"]],
    "SDP Queries" :[["sdp_general_query"],
                    ["sdp_bnep_query"]
                    ],
    "SPP Server"  : [["spp_counter"],
                     ["spp_flowcontrol"]],
    "BNEP/PAN"   : [["panu_demo"]],
    "Low Energy"  : [["gatt_browser"],
                    ["le_counter"]],
    "Dual Mode" : [["spp_and_le_counter"]],
}

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
    \item \emph{EXAMPLE_TITLE}: EXAMPLE_DESC, in Section \\ref{example:EXAMPLE_LABLE}.
"""
example_section = """
\subsection{EXAMPLE_TITLE: EXAMPLE_DESC}
\label{example:EXAMPLE_LABLE}

"""
example_subsection = """
\subsubsection{SECTION_TITLE}
"""

listing_start = """
$ $
\\begin{lstlisting}[caption= LISTING_CAPTION., label=listing:FILE_NAME:LISTING_LABLE]
"""

listing_ending = """\end{lstlisting}

"""

def replacePlaceholder(template, title, lable):
    snippet = template.replace("API_TITLE", title).replace("API_LABLE", lable)
    return snippet

def latexText(text, ref_prefix):
    if not text:
        return ""

    brief = text.replace("_","\_")
    # TODO: restore paths
        
    brief = brief.replace(" in the BTstack manual","")
    
    refs = re.match('.*Listing\s*\s*(\w+)(?:.|\s).*',brief)
    if refs:
        brief = brief.replace(refs.group(1), "\\ref{listing:"+ref_prefix+":" + refs.group(1)+"}")

    refs = re.match('.*(Section\s*)(\w*).*',brief, re.I)
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

def processTextLine(line, ref_prefix):
    if isTextTag(line):
        text_line_parts = re.match(".*(@text)(.*)", line)
        return " " + latexText(text_line_parts.group(2), ref_prefix)

    if isItemizeTag(line):
        text_line_parts = re.match("(\s*\*\s*\-\s*)(.*)", line)
        return "\n \item " + latexText(text_line_parts.group(2), ref_prefix)
    
    text_line_parts = re.match("(\s+\*\s+)(.*)", line)
    if text_line_parts:
        return " " + latexText(text_line_parts.group(2), ref_prefix)
    return ""

def getExampleTitle(example_path):
    example_title = ''
    with open(example_path, 'rb') as fin:
        for line in fin:
            parts = re.match('.*(EXAMPLE_START)\((.*)\):\s*(.*)(\*/)?\n',line)
            if parts: 
                example_title = parts.group(3).replace("_","\_")
                continue
    return example_title

class State:
    SearchExampleStart = 0
    SearchListingStart = 1
    SearchListingPause = 2
    SearchListingResume = 3
    SearchListingEnd = 4
    SearchItemizeEnd = 5
    ReachedExampleEnd = 6

def writeListings(aout, infile_name, ref_prefix):
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
                    title = latexText(parts.group(2), ref_prefix)
                    desc  = latexText(parts.group(3), ref_prefix)
                    aout.write(example_section.replace("EXAMPLE_TITLE", title).replace("EXAMPLE_DESC", desc).replace("EXAMPLE_LABLE", lable))
                    state = State.SearchListingStart
                continue
           
            # detect @section
            section_parts = re.match('.*(@section)\s*(.*)(:?\s*.?)\*?/?\n',line)
            if section_parts:
                aout.write("\n" + example_subsection.replace("SECTION_TITLE", section_parts.group(2)))
                continue

            # detect @subsection
            subsection_parts = re.match('.*(@section)\s*(.*)(:?\s*.?)\*?/?\n',line)
            if section_parts:
                subsubsection = example_subsection.replace("SECTION_TITLE", section_parts.group(2)).replace('section', 'subsection')
                aout.write("\n" + subsubsection)
                continue
            
            if isTextTag(line):
                text_block = processTextLine(line, ref_prefix)
                continue

            if text_block or itemize_block:
                if isEndOfComment(line) or isEmptyCommentLine(line):
                    if itemize_block:
                        # finish itemize
                        aout.write(itemize_block + "\n\end{itemize}\n")
                        itemize_block = None
                    else: 
                        if isEmptyCommentLine(line):
                            text_block = text_block + "\n\n"
                        else:
                            # finish text
                            aout.write(text_block)
                            text_block = None

                else:
                    if isNewItem(line) and not itemize_block:
                        # finish text, start itemize
                        aout.write(text_block)
                        text_block = None
                        itemize_block = "\n \\begin{itemize}"
                    
                    if itemize_block:
                        itemize_block = itemize_block + processTextLine(line, ref_prefix)
                    else:
                        # append text
                        text_block = text_block + processTextLine(line, ref_prefix)

                continue

            if state == State.SearchListingStart:
                parts = re.match('.*(LISTING_START)\((.*)\):\s*(.*\s*)(\*/).*',line)
                
                if parts: 
                    lst_lable = parts.group(2)
                    lst_caption = latexText(parts.group(3), ref_prefix)
                    listing = listing_start.replace("LISTING_CAPTION", lst_caption).replace("FILE_NAME", ref_prefix).replace("LISTING_LABLE", lst_lable)
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
        for group_title in list_of_groups:
            if not list_of_examples.has_key(group_title): continue
            examples = list_of_examples[group_title]
            for example in examples:
                example_path  = examples_folder + example[0] + ".c"
                example_title = getExampleTitle(example_path)
                example.append(example_title)
                    

        if standalone:
            aout.write(document_begin)
        aout.write(examples_header)
        aout.write("\n \\begin{itemize}\n");

        for group_title in list_of_groups:
            if not list_of_examples.has_key(group_title): continue
            examples = list_of_examples[group_title]
            
            group_title = group_title + " example"
            if len(examples) > 1:
                group_title = group_title + "s"
            group_title = group_title + ":"

            aout.write("  \item " + group_title + "\n");
            aout.write("  \\begin{itemize}\n");
            for example in examples:
                ref_prefix = example[0]
                title = latexText(example[0], ref_prefix)
                desc  = latexText(example[1], ref_prefix)
                aout.write(example_item.replace("EXAMPLE_TITLE", title).replace("EXAMPLE_DESC", desc).replace("EXAMPLE_LABLE", ref_prefix))
            aout.write("  \\end{itemize}\n")
        aout.write("\\end{itemize}\n")

        for group_title in list_of_groups:
            if not list_of_examples.has_key(group_title): continue
            examples = list_of_examples[group_title]
            
            for example in examples:
                file_name = examples_folder + example[0] + ".c"
                writeListings(aout, file_name, example[0])

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
