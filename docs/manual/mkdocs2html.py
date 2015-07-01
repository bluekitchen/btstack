#!/usr/bin/env python

import os, sys, shutil, re, pickle


def writeCodeBlock(aout, code, references):
    for function_name, url in references.items():
        html_link = '<a href="' + url + '">' + function_name + '</a>'
        #print "before:" + code + "\n\n"
        code = code.replace(function_name, html_link)
    aout.write(code)


def main(argv):
    html_path = "btstack/examples/examples/"
    html_tmppath = html_path + "tmp/"

    html_in  = html_path + "index.html"
    html_tmp = html_tmppath + "index.html"
    references = pickle.load(open( "tmp/references.p", "rb" ))
    
    os.mkdir(html_tmppath)

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
