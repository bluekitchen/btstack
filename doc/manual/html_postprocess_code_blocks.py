#!/usr/bin/env python3

import os, sys, shutil, re, pickle, getopt
from pathlib import Path

def writeCodeBlock(aout, code, references):
    for function_name, url in references.items():
        html_link = '<a href="' + url + '">' + function_name + '</a>'
        #print "before:" + code + "\n\n"
        code = code.replace(function_name, html_link)
    aout.write(code)


def main(argv):
    htmlfolder = "btstack/"
    
    cmd = 'html_postprocess_code_blocks.py [-o <htmlkfolder>]'
    
    try:
        opts, args = getopt.getopt(argv,"o:",["ofolder="])
    except getopt.GetoptError:
        print (cmd)
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print (cmd)
            sys.exit()
        elif opt in ("-o", "--ofolder"):
            htmlfolder = arg
        
    html_path = htmlfolder + "examples/"
    html_tmppath = htmlfolder + "examples/tmp/"

    html_in  = html_path + "examples/index.html"
    html_tmp = html_tmppath + "index.html"
    references = pickle.load(open( "references.p", "rb" ))

    Path(html_tmppath).mkdir(parents=True, exist_ok=True)
    
    codeblock = 0
    codeblock_end = 0

    with open(html_in, 'r') as fin:
        with open(html_tmp, 'w') as fout:
            for line in fin:
                if not codeblock:
                    fout.write(line)
                    if re.match('.*<pre><code>.*',line):
                        codeblock = 1
                    continue

                writeCodeBlock(fout,line, references)
                # check if codeblock ended
                if re.match('.*</code></pre>.*',line):
                    codeblock = 0
                    
    
    shutil.copyfile(html_tmp, html_in)
    shutil.rmtree(html_tmppath)

if __name__ == "__main__":
   main(sys.argv[1:])
