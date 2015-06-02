#!/usr/bin/env python
import getopt
import re
import sys

# Defines the names of example groups. Preserves the order in which the example groups will be parsed.
list_of_groups = ["iBeacon", "ANCS", "LE Central", "LE Peripheral"]

# Defines which examples belong to a group. Example is defined as [example file, example title].
list_of_examples = {
    "iBeacon": [["iBeacon"], ["iBeaconScanner"]],
    "ANCS": [["ANCS"]],
    "LE Central": [["LECentral"]],
    "LE Peripheral": [["LEPeripheral"]],
}

lst_header = """
"""

lst_ending = """
"""

examples_header = """
"""

example_item = """
    - [EXAMPLE_TITLE](#section:EXAMPLE_LABEL): EXAMPLE_DESC.
"""
example_section = """

## EXAMPLE_TITLE: EXAMPLE_DESC
<a name="section:EXAMPLE_LABEL"></a>

"""
example_subsection = """
### SECTION_TITLE
"""

listing_start = """

<a name="FILE_NAME:LISTING_LABEL"></a>
<!-- -->

"""

listing_ending = """

"""


def replacePlaceholder(template, title, lable):
    snippet = template.replace("API_TITLE", title).replace("API_LABEL", lable)
    return snippet


def latexText(text, ref_prefix):
    if not text:
        return ""
    brief = text.replace(" in the BTstack manual","")

    refs = re.match('.*(Listing\s+)(\w+).*',brief)
    if refs:
        brief = brief.replace(refs.group(1), "[code snippet below]")
        brief = brief.replace(refs.group(2), "(#"+ref_prefix+":" + refs.group(2)+")")

    refs = re.match('.*(Section\s+)(\w+).*',brief)
    if refs:
        brief = brief.replace(refs.group(1), "[here]")
        brief = brief.replace(refs.group(2), "(#section:"+refs.group(2)+")")

    return brief


def isEmptyCommentLine(line):
    return re.match('(\s*\*\s*)\n',line)


def isCommentLine(line):
    return re.match('(\s*\*\s*).*',line)


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
        return "\n- " + latexText(text_line_parts.group(2), ref_prefix)
    
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

text_block = ''
itemize_block = ''

def writeTextBlock(aout, lstStarted):
    global text_block
    if text_block and not lstStarted:
        aout.write(text_block)
        text_block = ''

def writeItemizeBlock(aout, lstStarted):
    global itemize_block
    if itemize_block and not lstStarted:
        aout.write(itemize_block + "\n\n")
        itemize_block = ''

def writeListings(aout, infile_name, ref_prefix):
    global text_block, itemize_block
    itemText = None
    state = State.SearchExampleStart
    code_in_listing = ""
    code_identation = "    "
    skip_code = 0

    with open(infile_name, 'rb') as fin:
        for line in fin:
            if state == State.SearchExampleStart:
                parts = re.match('.*(EXAMPLE_START)\((.*)\):\s*(.*)(\*/)?\n',line)
                if parts: 
                    lable = parts.group(2).replace("_","")
                    title = latexText(parts.group(2), ref_prefix)
                    desc  = latexText(parts.group(3), ref_prefix)
                    aout.write(example_section.replace("EXAMPLE_TITLE", title).replace("EXAMPLE_DESC", desc).replace("EXAMPLE_LABEL", lable))
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
                text_block = text_block + "\n\n" + processTextLine(line, ref_prefix)
                continue

            skip_code = 0
            lstStarted = state != State.SearchListingStart
            if text_block or itemize_block:
                if isEndOfComment(line) or isEmptyCommentLine(line):
                    skip_code = 1
                    if itemize_block:
                        # finish itemize
                        writeItemizeBlock(aout, lstStarted)
                    else: 
                        if isEmptyCommentLine(line):
                            text_block = text_block + "\n\n"
                            
                        else:
                            writeTextBlock(aout, lstStarted)


                else:
                    if isNewItem(line) and not itemize_block:
                        skip_code = 1
                        # finish text, start itemize
                        writeTextBlock(aout, lstStarted)
                        itemize_block = "\n " + processTextLine(line, ref_prefix)
                        continue
                    if itemize_block:
                        skip_code = 1
                        itemize_block = itemize_block + processTextLine(line, ref_prefix)
                    elif isCommentLine(line):
                        # append text
                        skip_code = 1
                        text_block = text_block + processTextLine(line, ref_prefix)
                    else:
                        skip_code = 0
                #continue

            if state == State.SearchListingStart:
                parts = re.match('.*(LISTING_START)\((.*)\):\s*(.*)(\s+\*/).*',line)
                
                if parts: 
                    lst_lable = parts.group(2).replace("_","")
                    lst_caption = latexText(parts.group(3), ref_prefix)
                    listing = listing_start.replace("LISTING_CAPTION", lst_caption).replace("FILE_NAME", ref_prefix).replace("LISTING_LABEL", lst_lable)
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
                    writeItemizeBlock(aout, 0)
                    writeTextBlock(aout, 0)
                elif parts_pause:
                    code_in_listing = code_in_listing + code_identation + "...\n"
                    state = State.SearchListingResume
                elif not end_comment_parts:
                    # aout.write(line)
                    if not skip_code:
                        code_in_listing = code_in_listing + code_identation + line.replace("    ", "  ")
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
                writeItemizeBlock(aout, 0)
                writeTextBlock(aout, 0)
                state = State.ReachedExampleEnd
                print "Reached end of the example"
            

# write list of examples
def processExamples(intro_file, examples_folder, examples_ofile):
    with open(examples_ofile, 'w') as aout:

        with open(intro_file, 'rb') as fin:
            for line in fin:
                aout.write(line)

        for group_title in list_of_groups:
            if not list_of_examples.has_key(group_title): continue
            examples = list_of_examples[group_title]
            for example in examples:
                example_path  = examples_folder + example[0] + "/" + example[0] + ".ino"
                example_title = getExampleTitle(example_path)
                example.append(example_title)
                    
        aout.write(examples_header)
        aout.write("\n\n");

        for group_title in list_of_groups:
            if not list_of_examples.has_key(group_title): continue
            examples = list_of_examples[group_title]
            
            group_title = group_title + " example"
            if len(examples) > 1:
                group_title = group_title + "s"
            group_title = group_title + ":"

            aout.write("- " + group_title + "\n");
            for example in examples:
                ref_prefix = example[0].replace("_", "")
                title = latexText(example[0], ref_prefix)
                desc  = latexText(example[1], ref_prefix)
                aout.write(example_item.replace("EXAMPLE_TITLE", title).replace("EXAMPLE_DESC", desc).replace("EXAMPLE_LABEL", ref_prefix))
            aout.write("\n")
        aout.write("\n")

        for group_title in list_of_groups:
            if not list_of_examples.has_key(group_title): continue
            examples = list_of_examples[group_title]
            
            for example in examples:
                file_name = examples_folder + example[0] + "/" + example[0] + ".ino"
                writeListings(aout, file_name, example[0].replace("_",""))


def main(argv):
    btstack_folder = "../../../"
    docs_folder = "docs/examples/"
    inputfolder = btstack_folder + "platforms/arduino/examples/"
    outputfile = docs_folder + "generated.md"
    intro_file = "docs/examples/intro.md"

    try:
        opts, args = getopt.getopt(argv,"hiso:",["ifolder=","ofile="])
    except getopt.GetoptError:
        print 'update_listings.py [-i <inputfolder>] [-o <outputfile>]'
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print 'update_listings.py [-i <inputfolder>] [-s] [-o <outputfile>]'
            sys.exit()
        elif opt in ("-i", "--ifolder"):
            inputfolder = arg
        elif opt in ("-o", "--ofile"):
            outputfile = arg
    print 'Input folder is ', inputfolder
    print 'Output file is ', outputfile
    processExamples(intro_file, inputfolder, outputfile)

if __name__ == "__main__":
   main(sys.argv[1:])
