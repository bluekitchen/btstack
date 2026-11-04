#!/usr/bin/env python3

import os, sys, shutil, re, pickle, getopt
from pathlib import Path

CODE_BLOCK_RE = re.compile(r'(<pre><code(?:\s[^>]*)?>)(.*?)(</code></pre>)', re.DOTALL)


def postprocess_code_blocks(html, references=None, protect_arrow_operators=False):
    def replace_code_block(match):
        code = match.group(2)

        if references:
            for function_name, url in references.items():
                html_link = '<a href="' + url + '">' + function_name + '</a>'
                code = code.replace(function_name, html_link)

        if protect_arrow_operators:
            code = code.replace('-&gt;', '<span class="code-operator">-&gt;</span>')

        return match.group(1) + code + match.group(3)

    return CODE_BLOCK_RE.sub(replace_code_block, html)


def main(argv):
    htmlfolder = "btstack/"
    html_in = None
    html_out = None
    protect_arrow_operators = False
    
    cmd = 'html_postprocess_code_blocks.py [-o <htmlfolder>] [--input <html_in> --output <html_out> --protect-arrow-operators]'
    
    try:
        opts, args = getopt.getopt(argv,"o:",["ofolder=", "input=", "output=", "protect-arrow-operators"])
    except getopt.GetoptError:
        print (cmd)
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print (cmd)
            sys.exit()
        elif opt in ("-o", "--ofolder"):
            htmlfolder = arg
        elif opt == "--input":
            html_in = arg
        elif opt == "--output":
            html_out = arg
        elif opt == "--protect-arrow-operators":
            protect_arrow_operators = True

    if (html_in is None) != (html_out is None):
        print("--input and --output must be used together")
        sys.exit(2)

    if html_in is not None:
        with open(html_in, 'r') as fin:
            html = fin.read()
        with open(html_out, 'w') as fout:
            fout.write(postprocess_code_blocks(html, protect_arrow_operators=protect_arrow_operators))
        return
        
    html_path = htmlfolder + "examples/"
    html_tmppath = htmlfolder + "examples/tmp/"

    html_in  = html_path + "examples/index.html"
    html_tmp = html_tmppath + "index.html"
    references = pickle.load(open( "references.p", "rb" ))

    Path(html_tmppath).mkdir(parents=True, exist_ok=True)
    
    with open(html_in, 'r') as fin:
        with open(html_tmp, 'w') as fout:
            fout.write(postprocess_code_blocks(fin.read(), references=references))
    
    shutil.copyfile(html_tmp, html_in)
    shutil.rmtree(html_tmppath)

if __name__ == "__main__":
   main(sys.argv[1:])
