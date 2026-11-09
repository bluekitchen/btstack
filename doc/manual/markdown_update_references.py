#!/usr/bin/env python3

import sys, os, shutil, getopt
import re, yaml
import subprocess

githuburl = "https://github.com/bluekitchen/btstack/tree/"
gitbranchname = "master"

# helper to write anchors and references
def insert_anchor(mdout, reference):
    anchor = "<a name=\"" + reference + "\"></a>\n\n"
    mdout.write(anchor)

def insert_reference(mdout, text, link):
    mdout.write("")

def process_source_file_link(mdin, mdout, githuburl, line):
    parts = re.match(r'.*(GITHUB_URL).*\n',line)
    if parts:
        line_with_source_file_link = line.replace("GITHUB_URL", githuburl)
        mdout.write(line_with_source_file_link)
        line = ''
    return line

def process_relative_links(source_file, line):
    def add_markdown_extension(match):
        prefix, url, suffix = match.groups()

        # Preserve anchors, external URLs, and links that already name a file.
        if url.startswith(("#", "/", "http://", "https://", "mailto:")):
            return match.group(0)

        target, separator, fragment = url.partition("#")
        if not target or target.endswith(".md"):
            return match.group(0)

        # The templates use MkDocs site-style links (e.g. ../profiles/#anchor),
        # while MkDocs validates source links against Markdown files.
        target_without_slash = target.rstrip("/")
        source_dir = os.path.dirname(source_file)
        candidates = [
            (os.path.normpath(os.path.join(source_dir, target_without_slash + ".md")), target_without_slash + ".md")
        ]

        # Root-level pages historically used ../foo/ for the site's /foo/
        # route. In Markdown, the corresponding source file is foo.md in the
        # same directory.
        if target_without_slash.startswith("../"):
            candidates.append((
                os.path.normpath(os.path.join(source_dir, target_without_slash[3:] + ".md")),
                target_without_slash[3:] + ".md"
            ))

        for candidate, updated_target in candidates:
            if os.path.isfile(candidate):
                updated_url = updated_target + (separator + fragment if separator else "")
                return prefix + updated_url + suffix

        return match.group(0)

    return re.sub(r'(\[[^\]]+\]\()([^)\s]+)(\))', add_markdown_extension, line)

def process_listing_links(line):
    # MkDocs 1.5 does not validate legacy <a name="lst:..."> targets. Render
    # their links as HTML and let markdown_validate_links.py validate them.
    return re.sub(r'\[([^\]]+)\]\((#lst:[^\)\s]+)\)', r'<a href="\2">\1</a>', line)

# handlers for various elements
def process_section(mdin, mdout, line):
    section = re.match(r'(#+.*){#(sec:.*)}',line)
    if section:
        # Keep explicit heading IDs as Markdown attributes.  With MkDocs'
        # attr_list extension these are both rendered and validated as anchors.
        mdout.write(line)
        line = ''
    return line

def process_figure(mdin, mdout, line):
    # detect figure
    figure = re.match(r'\s*(\!.*)({#(fig:.*)})',line)
    if figure:
        insert_anchor(mdout, figure.group(3))
        mdout.write(figure.group(1)+"\n")
        line = ''
    return line

def process_fig_ref(mdin, mdout, line):
    # detect figure reference
    figure_ref = re.match(r'.*({@(fig:.*)})',line)
    if figure_ref:
        md_reference = "[below](#"+figure_ref.group(2)+")"
        line = line.replace(figure_ref.group(1), md_reference) 
        mdout.write(line)
        line = ''
    return line

def process_table(mdin, mdout, line):
    # detect table
    table = re.match(r'\s*(Table:.*)({#(tbl:.*)})',line)
    if table:
        insert_anchor(mdout, table.group(3))
        mdout.write(table.group(1)+"\n")
        line = ''
    return line

def process_tbl_ref(mdin, mdout, line):
    table_ref = re.match(r'.*({@(tbl:.*)})',line)
    if table_ref:
        md_reference = "[below](#"+table_ref.group(2)+")"
        line = line.replace(table_ref.group(1), md_reference) 
        mdout.write(line)
        line = ''
    return line

def process_listing(mdin, mdout, line):
    listing_start = re.match(r'.*{#(lst:.*)\s+.c\s+.*',line)
    listing_end   = re.match(r'\s*~~~~\s*\n',line)
    if listing_start:
        insert_anchor(mdout, listing_start.group(1))
        line = ''
    elif listing_end:
        mdout.write("\n")
        line = ''
    return line

def process_file(mk_file, markdownfolder, mkdocsfolder, githuburl):
    source_file = markdownfolder +"/"+ mk_file
    dest_file   = mkdocsfolder +"/"+ mk_file
    # print("Processing %s -> %s" % (source_file, dest_file))

    with open(dest_file, 'wt') as mdout:
        with open(source_file, 'rt') as mdin:
            for line in mdin:
                line = process_relative_links(source_file, line)
                line = process_listing_links(line)
                line = process_section(mdin, mdout, line)
                if len(line) == 0:
                    continue
                line = process_source_file_link(mdin, mdout, githuburl, line)
                if len(line) == 0:
                    continue
                line = process_figure(mdin, mdout, line)
                if len(line) == 0:
                    continue
                line = process_fig_ref(mdin, mdout, line)
                if len(line) == 0:
                    continue
                line = process_table(mdin, mdout, line)
                if len(line) == 0:
                    continue
                line = process_tbl_ref(mdin, mdout, line)
                if len(line) == 0:
                    continue
                line = process_listing(mdin, mdout, line)
                if len(line) == 0:
                    continue
                mdout.write(line)

def main(argv):
    global gitbranchname
    markdownfolder = "docs-markdown/"
    mkdocsfolder = "docs/"
    
    cmd = 'markdown_update_references.py [-i <markdownfolder>] [-o <mkdocsfolder>] [-g <githuburl>]'

    try:
        opts, args = getopt.getopt(argv,"i:o:g:",["ifolder=","ofolder=","github="])
    except getopt.GetoptError:
        print (cmd)
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print (cmd)
            sys.exit()
        elif opt in ("-i", "--ifolder"):
            markdownfolder = arg
        elif opt in ("-o", "--ofolder"):
            mkdocsfolder = arg
        elif opt in ("-g", "--github"):
            githuburl = arg

    try:
        output = subprocess.check_output("git symbolic-ref --short HEAD", stderr=subprocess.STDOUT, timeout=3, shell=True)
        gitbranchname = output.decode().rstrip()
    except subprocess.CalledProcessError as exc:
        print('GIT branch name: failed to get, use default value \"%s\""  ', gitbranchname, exc.returncode, exc.output)
    else:
        print('GIT branch name:  %s' % gitbranchname)
        
    githuburl = githuburl + gitbranchname
    print('GITHUB URL:       %s\n' % githuburl)

    # Process both the split HTML pages and the combined PDF pages.
    files = set()
    for yml_file in ("mkdocs.yml", "mkdocs-latex.yml"):
        with open(yml_file) as yin:
            doc = yaml.safe_load(yin)
        for page in doc["nav"]:
            value = list(page.values())[0]
            if isinstance(value, str):
                files.add(value)
            elif isinstance(value, list):
                files.update(list(item.values())[0] for item in value)
    for filepath in sorted(files):
        process_file(filepath, markdownfolder, mkdocsfolder, githuburl)

                
if __name__ == "__main__":
   main(sys.argv[1:])
