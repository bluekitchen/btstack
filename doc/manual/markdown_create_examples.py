#!/usr/bin/env python3
import os, sys, getopt, re
import subprocess



# Defines the names of example groups. Preserves the order in which the example groups will be parsed.
list_of_groups = [
    "Hello World", 
    "GAP", 
    
    "Low Energy", 
    "Performance", 
    
    "Audio", 
    "SPP Server", 
    "Networking", 
    "HID", 

    "Dual Mode", 
    "SDP Queries", 
    "Phone Book Access", 
    
    "Testing"
]

# Defines which examples belong to a group. Example is defined as [example file, example title].
list_of_examples = { 
    "Audio"       : [["a2dp_sink_demo"],["a2dp_source_demo"], ["avrcp_browsing_client"], 
                     ["hfp_ag_demo"], ["hfp_hf_demo"], 
                     ["hsp_ag_demo"], ["hsp_hs_demo"],
                     ["sine_player"], ["mod_player"], ["audio_duplex"]],
    
    "Dual Mode"   : [["spp_and_gatt_counter"], ["gatt_streamer_server"]],
    
    "GAP"         : [["gap_inquiry"], ["gap_link_keys"]],
    
    "Hello World" : [["led_counter"]],
    "HID"         : [["hid_keyboard_demo"], ["hid_mouse_demo"], ["hid_host_demo"], ["hog_keyboard_demo"], ["hog_mouse_demo"], ["hog_boot_host_demo"]],
    
    "Low Energy"  : [["gap_le_advertisements"], ["gatt_browser"], ["gatt_counter"], ["gatt_streamer_server"], 
                    ["gatt_battery_query"], ["gatt_device_information_query"], ["gatt_heart_rate_client"], 
                    ["nordic_spp_le_counter"], ["nordic_spp_le_streamer"], ["ublox_spp_le_counter"], 
                    ["sm_pairing_central"], ["sm_pairing_peripheral"], 
                    ["le_credit_based_flow_control_mode_client"], ["le_credit_based_flow_control_mode_server"], 
                    ["att_delayed_response"], ["ancs_client_demo"], ["le_mitm"]],
    
    "Networking"    :  [["pan_lwip_http_server"], ["panu_demo"]],
    
    "Performance" : [["le_streamer_client"], ["gatt_streamer_server"], ["le_credit_based_flow_control_mode_client"], ["le_credit_based_flow_control_mode_server"], ["spp_streamer_client"], ["spp_streamer"]],
    "Phone Book Access" : [["pbap_client_demo"]],
    
    "SDP Queries" : [["sdp_general_query"], ["sdp_rfcomm_query"], ["sdp_bnep_query"]],
    "SPP Server"  : [["spp_counter"], ["spp_flowcontrol"]],    
    "Testing"     : [["dut_mode_classic"]]
}

lst_header = """
"""

lst_ending = """
"""

examples_header = """
"""

example_item = """
    - [EXAMPLE_TITLE](#sec:EXAMPLE_LABELExample): EXAMPLE_DESC.
"""
example_section = """

## EXAMPLE_DESC {#sec:EXAMPLE_LABELExample}

Source Code: [EXAMPLE_TITLE.c](https://github.com/bluekitchen/btstack/tree/GIT_BRANCH/example/EXAMPLE_TITLE.c)

"""
example_subsection = """
### SECTION_TITLE
"""

listing_reference = """[here](#lst:FILE_NAMELISTING_LABEL)
"""

listing_start = """

~~~~ {#lst:FILE_NAMELISTING_LABEL .c caption="{LISTING_CAPTION}"}

"""

listing_ending = """
~~~~

"""

def replacePlaceholder(template, title, label):
    snippet = template.replace("API_TITLE", title).replace("API_LABEL", label)
    return snippet

def latexText(text, ref_prefix):
    if not text:
        return ""
    brief = text.replace(" in the BTstack manual","")

    refs = re.match(r'.*(Listing\s+)(\w+).*',brief)
    if refs:
        brief = brief.replace(refs.group(2), "[here](#lst:"+ref_prefix + refs.group(2)+")")

    return brief

def isEmptyCommentLine(line):
    return re.match(r'(\s*\*\s*)\n',line)

def isCommentLine(line):
    return re.match(r'(\s*\*\s*).*',line)

def isEndOfComment(line):
    return re.match(r'\s*\*/.*', line)

def isNewItem(line):
    return re.match(r'(\s*\*\s*\-\s*)(.*)',line)

def isTextTag(line):
    return re.match(r'.*(@text).*', line)

def isItemizeTag(line):
    return re.match(r'(\s+\*\s+)(-\s)(.*)', line)

def processTextLine(line, ref_prefix):
    if isTextTag(line):
        text_line_parts = re.match(r'.*(@text)(.*)', line)
        return " " + latexText(text_line_parts.group(2), ref_prefix)

    if isItemizeTag(line):
        text_line_parts = re.match(r'(\s*\*\s*\-\s*)(.*)', line)
        return "\n- " + latexText(text_line_parts.group(2), ref_prefix)
    
    text_line_parts = re.match(r'(\s+\*\s+)(.*)', line)
    if text_line_parts:
        return " " + latexText(text_line_parts.group(2), ref_prefix)
    return ""

def getExampleTitle(example_path):
    example_title = ''
    with open(example_path, 'r') as fin:
        for line in fin:
            parts = re.match(r'.*(EXAMPLE_START)\((.*)\):\s*(.*)(\*/)?\n',line)
            if parts: 
                example_title = parts.group(3).replace("_",r'\_')
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

def writeListings(aout, infile_name, ref_prefix, git_branch_name):
    global text_block, itemize_block
    itemText = None
    state = State.SearchExampleStart
    code_in_listing = ""
    code_identation = "    "
    skip_code = 0

    with open(infile_name, 'r') as fin:
        for line in fin:
            if state == State.SearchExampleStart:
                parts = re.match(r'.*(EXAMPLE_START)\((.*)\):\s*(.*)(\*/)?\n',line)
                if parts: 
                    label = parts.group(2).replace("_","")
                    title = latexText(parts.group(2), ref_prefix)
                    desc  = latexText(parts.group(3), ref_prefix)
                    aout.write(example_section.replace("EXAMPLE_TITLE", title).replace("EXAMPLE_DESC", desc).replace("EXAMPLE_LABEL", label).replace("GIT_BRANCH", git_branch_name))
                    state = State.SearchListingStart
                continue
           
            # detect @section
            section_parts = re.match(r'.*(@section)\s*(.*)(:?\s*.?)\*?/?\n',line)
            if section_parts:
                aout.write("\n" + example_subsection.replace("SECTION_TITLE", section_parts.group(2)))
                continue

            # detect @subsection
            subsection_parts = re.match(r'.*(@section)\s*(.*)(:?\s*.?)\*?/?\n',line)
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
                parts = re.match(r'.*(LISTING_START)\((.*)\):\s*(.*)(\s+\*/).*',line)
                
                if parts: 
                    lst_label = parts.group(2).replace("_","")
                    lst_caption = latexText(parts.group(3), ref_prefix)
                    listing = listing_start.replace("LISTING_CAPTION", lst_caption).replace("FILE_NAME", ref_prefix).replace("LISTING_LABEL", lst_label)
                    if listing:
                        aout.write("\n" + listing)
                    state = State.SearchListingEnd
                continue
            
            if state == State.SearchListingEnd:
                parts_end = re.match(r'.*(LISTING_END).*',line)
                parts_pause = re.match(r'.*(LISTING_PAUSE).*',line)
                end_comment_parts = re.match(r'.*(\*/)\s*\n', line);
                
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
                parts = re.match(r'.*(LISTING_RESUME).*',line)
                if parts:
                    state = State.SearchListingEnd
                continue
        
            parts = re.match(r'.*(EXAMPLE_END).*',line)
            if parts:
                if state != State.SearchListingStart:
                    print("Formating error detected")
                writeItemizeBlock(aout, 0)
                writeTextBlock(aout, 0)
                state = State.ReachedExampleEnd
                print("Reached end of the example")
            

# write list of examples
def processExamples(intro_file, examples_folder, examples_ofile, git_branch_name):
    with open(examples_ofile, 'w') as aout:
        with open(intro_file, 'r') as fin:
            for line in fin:
                aout.write(line)

        for group_title in list_of_groups:
            if not group_title in list_of_examples: continue
            examples = list_of_examples[group_title]
            for example in examples:
                example_path  = examples_folder + example[0] + ".c"
                example_title = getExampleTitle(example_path)
                example.append(example_title)
                    
        aout.write(examples_header)
        aout.write("\n\n");

        for group_title in list_of_groups:
            if not group_title in list_of_examples: continue
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
            if not group_title in list_of_examples: continue
            examples = list_of_examples[group_title]
            
            for example in examples:
                file_name = examples_folder + example[0] + ".c"
                writeListings(aout, file_name, example[0].replace("_",""), git_branch_name)


def main(argv):
    btstackfolder = "../../"
    git_branch_name = "master"

    cmd = 'markdown_create_examples.py [-r <root_btstackfolder>] [-t <templatefolder>] [-o <output_markdownfolder>]'
    
    try:
        opts, args = getopt.getopt(argv,"r:t:o:",["rfolder=","tfolder=","ofolder="])
    except getopt.GetoptError:
        print (cmd)
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print (cmd)
            sys.exit()
        elif opt in ("-r", "--rfolder"):
            btstackfolder = arg
        elif opt in ("-t", "--tfolder"):
            templatefolder = arg
        elif opt in ("-o", "--ofolder"):
            markdownfolder = arg


    examples_folder    = btstackfolder + "example/"
    examples_introfile = templatefolder + "examples_intro.md"
    outputfile         = markdownfolder + "examples/examples.md"

    print ('Input folder: ', examples_folder)
    print ('Intro file:   ', examples_introfile)
    print ('Output file:  ', outputfile)
    
    try:
        output = subprocess.check_output("git symbolic-ref --short HEAD", stderr=subprocess.STDOUT, timeout=3, shell=True)
        git_branch_name = output.decode().rstrip()
    except subprocess.CalledProcessError as exc:
        print('GIT branch name: failed to get, use default value \"%s\""  ', git_branch_name, exc.returncode, exc.output)
    else:
        print ('GIT branch name :  %s' % git_branch_name)
    
    processExamples(examples_introfile, examples_folder, outputfile, git_branch_name)

if __name__ == "__main__":
   main(sys.argv[1:])
