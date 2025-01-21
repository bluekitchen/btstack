#!/usr/bin/env python3
import os, sys, getopt, re, pickle
import subprocess

class State:
    SearchTitle = 0
    SearchEndTitle = 1
    SearchStartAPI = 2
    SearchEndAPI = 3
    DoneAPI = 4

header_files = {}
functions = {}
typedefs = {}

linenr = 0
typedefFound = 0
multiline_function_def = 0
state = State.SearchStartAPI

# if dash is used in api_header, the windmill theme will repeat the same API_TITLE twice in the menu (i.e: APIs/API_TITLE/API_TITLE)
# if <h2> is used, this is avoided (i.e: APIs/API_TITLE), but reference {...} is not translated to HTML
api_header = """
# API_TITLE API {#sec:API_LABEL_api}

"""

api_subheader = """
## API_TITLE API {#sec:API_LABEL_api}

"""

api_description = """
**FILENAME** DESCRIPTION

"""

code_ref = """GITHUB/FPATH#LLINENR"""

def isEndOfComment(line):
    parts = re.match(r'\s*\*\/.*', line)
    if parts:
        return True
    return False

def isStartOfComment(line):
    parts = re.match(r'\s*\/\*\*.*', line)
    if parts:
        return True
    return False

def isComment(line): 
    parts = re.match(r'\s*\/\/.*', line)
    if parts:
        return True
    parts = re.match(r'\s*\*.*', line)
    if parts:
        return True

    return isStartOfComment(line) or isEndOfComment(line)

def isTypedefStart(line):
    return re.match(r'.*typedef\s+struct.*', line)

def codeReference(fname, githuburl, filename_without_extension, filepath, linenr):
    global code_ref
    ref = code_ref.replace("GITHUB", githuburl)
    ref = ref.replace("FPATH", filename_without_extension)
    ref = ref.replace("LINENR", str(linenr))
    return ref

def isTagAPI(line):
    return re.match(r'(.*)(-\s*\')(APIs).*',line)

def getSecondLevelIdentation(line):
    indentation = ""
    parts = re.match(r'(.*)(-\s*\')(APIs).*',line)
    if parts:
        # return double identation for the submenu
        indentation = parts.group(1) + parts.group(1) + "- "
    return indentation

def filename_stem(filepath):
    return os.path.splitext(os.path.basename(filepath))[0]

def writeAPI(fout, fin, mk_codeidentation):
    state = State.SearchStartAPI
    
    for line in fin:
        if state == State.SearchStartAPI:
            parts = re.match(r'.*API_START.*',line)
            if parts:
                state = State.SearchEndAPI
            continue
        
        if state == State.SearchEndAPI:
            parts = re.match(r'.*API_END.*',line)
            if parts:
                state = State.DoneAPI
                continue
            fout.write(mk_codeidentation + line)
            continue



def createIndex(fin, filename, api_filepath, api_title, api_label, githuburl):
    global typedefs, functions
    global linenr, multiline_function_def, typedefFound, state
    
    
    for line in fin:
        if state == State.DoneAPI:
            continue

        linenr = linenr + 1
        
        if state == State.SearchStartAPI:
            parts = re.match(r'.*API_START.*',line)
            if parts:
                state = State.SearchEndAPI
            continue
        
        if state == State.SearchEndAPI:
            parts = re.match(r'.*API_END.*',line)
            if parts:
                state = State.DoneAPI
                continue

        if isComment(line):
            continue

        param = re.match(r".*@brief.*", line)
        if param:
            continue
        param = re.match(r".*@param.*", line)
        if param:
            continue
        param = re.match(r".*@return.*", line)
        if param:
            continue
        param = re.match(r".*@result.*", line)
        if param:
            continue
        param = re.match(r".*@note.*", line)
        if param:
            continue
        param = re.match(r".*return.*", line)
        if param:
            continue
        
        # search typedef struct begin
        if isTypedefStart(line):
            typedefFound = 1
        
        # search typedef struct end
        if typedefFound:
            typedef = re.match(r'}\s*(.*);\n', line)
            if typedef:
                typedefFound = 0
                typedefs[typedef.group(1)] = codeReference(typedef.group(1), githuburl, filename, api_filepath, linenr)
            continue

        ref_function =  re.match(r'.*typedef\s+void\s+\(\s*\*\s*(.*?)\)\(.*', line)
        if ref_function:
            functions[ref_function.group(1)] = codeReference(ref_function.group(1), githuburl, filename, api_filepath, linenr)
            continue

        # filter callback 
        callback_function_definition = re.match(r'(.*?)\s*\(\s*\*.*\(*.*;\n', line)
        if callback_function_definition:
            continue

        one_line_function_definition = re.match(r'(.*?)\s*\(.*\(*.*;\n', line)
        if one_line_function_definition:
            parts = one_line_function_definition.group(1).split(" ")
            name = parts[len(parts)-1]
            if len(name) == 0:
                print(parts)
                sys.exit(10)
            # ignore typedef for callbacks
            if parts[0] == 'typedef':
                continue
            functions[name] = codeReference( name, githuburl, filename, api_filepath, linenr)
            continue

        multi_line_function_definition = re.match(r'.(.*?)\s*\(.*\(*.*', line)
        if multi_line_function_definition:
            parts = multi_line_function_definition.group(1).split(" ")

            name = parts[len(parts)-1]
            if len(name) == 0:
                print(parts)
                sys.exit(10)
            multiline_function_def = 1
            functions[name] = codeReference(name, githuburl, filename, api_filepath, linenr)


def findTitle(fin):
    title = None
    desc = ""
    state = State.SearchTitle
    
    for line in fin:
        if state == State.SearchTitle:
            if isStartOfComment(line):
                continue

            parts = re.match(r'.*(@title)(.*)', line)
            if parts:
                title = parts.group(2).strip()
                state = State.SearchEndTitle
                continue
    
        if state == State.SearchEndTitle:
            if (isEndOfComment(line)):
                state = State.DoneAPI
                break

            parts = re.match(r'(\s*\*\s*)(.*\n)',line)
            if parts:
                desc = desc + parts.group(2)
    return [title, desc]

def main(argv):
    global linenr, multiline_function_def, typedefFound, state
    
    mk_codeidentation = "    "
    git_branch_name = "master"
    btstackfolder = "../../"
    githuburl_template  = "https://github.com/bluekitchen/btstack/blob/"
    githuburl = "master/src/"
    markdownfolder = "docs-markdown/"
    
    cmd = 'markdown_create_apis.py [-r <root_btstackfolder>] [-g <githuburl>] [-o <output_markdownfolder>]'
    try:
        opts, args = getopt.getopt(argv,"r:g:o:",["rfolder=","github=","ofolder="])
    except getopt.GetoptError:
        print (cmd)
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print (cmd)
            sys.exit()
        elif opt in ("-r", "--rfolder"):
            btstackfolder = arg
        elif opt in ("-g", "--github"):
            githuburl = arg
        elif opt in ("-o", "--ofolder"):
            markdownfolder = arg

    apifile   = markdownfolder + "appendix/apis.md"
    # indexfile = markdownfolder + "api_index.md"
    btstack_srcfolder = btstackfolder + "src/"

    try:
        output = subprocess.check_output("git symbolic-ref --short HEAD", stderr=subprocess.STDOUT, timeout=3, shell=True)
        git_branch_name = output.decode().rstrip()
    except subprocess.CalledProcessError as exc:
        print('GIT branch name: failed to get, use default value \"%s\""  ', git_branch_name, exc.returncode, exc.output)
    else:
        print ('GIT branch name :  %s' % git_branch_name)

    githuburl = githuburl_template + git_branch_name

    print ('BTstack src folder is : ' + btstack_srcfolder)
    print ('API file is       : ' + apifile)
    print ('Github URL is    : ' +  githuburl)
    
    # create a dictionary of header files {file_path : [title, description]}
    # title and desctiption are extracted from the file
    for root, dirs, files in os.walk(btstack_srcfolder, topdown=True):
        for f in files:
            if not f.endswith(".h"):
                continue

            if not root.endswith("/"):
                root = root + "/"

            header_filepath = root + f
            
            with open(header_filepath, 'rt') as fin:
                [header_title, header_desc] = findTitle(fin)

            if header_title:
                header_files[header_filepath] = [header_title, header_desc]
            else:
                print("No @title flag found. Skip %s" % header_filepath)
    
    # create an >md file, for each header file in header_files dictionary
    for header_filepath in sorted(header_files.keys()):
        filename = str(header_filepath)
        filename = filename.split("../../")[1]

        filename_without_extension = filename_stem(header_filepath) # file name without .h
        filetitle = header_files[header_filepath][0]
        description = header_files[header_filepath][1]

        if len(description) > 1:
            description = ": " + description

        header_description = api_description.replace("FILENAME", filename).replace("DESCRIPTION", description)
        
        header_title = api_header.replace("API_TITLE", filetitle).replace("API_LABEL", filename_without_extension)
        markdown_filepath = markdownfolder + "appendix/" + filename_without_extension + ".md"

        with open(header_filepath, 'rt') as fin:
            with open(markdown_filepath, 'wt') as fout:
                fout.write(header_title)
                fout.write(header_description)
                writeAPI(fout, fin, mk_codeidentation)

        with open(header_filepath, 'rt') as fin:
            linenr = 0
            typedefFound = 0
            multiline_function_def = 0
            state = State.SearchStartAPI
            createIndex(fin, filename, markdown_filepath, header_title, filename_without_extension, githuburl)
     
    # add API list to the navigation menu           
    with open("mkdocs-temp.yml", 'rt') as fin:
        with open("mkdocs.yml", 'wt') as fout:
            for line in fin:
                fout.write(line)

                if not isTagAPI(line):
                    continue

                identation = getSecondLevelIdentation(line)
               
                for header_filepath in sorted(header_files.keys()):
                    header_title = os.path.basename(header_filepath)
                    markdown_reference = "appendix/" + filename_stem(header_filepath) + ".md"
                    
                    fout.write(identation + "'" + header_title + "': " + markdown_reference + "\n")

    
    # create mkdocs-latex.yml with single appendix/apis.md reference for pdf generation
    with open("mkdocs-temp.yml", 'rt') as fin:
        with open("mkdocs-latex.yml", 'wt') as fout:
            for line in fin:
                if not isTagAPI(line):
                    fout.write(line)
                    continue

                fout.write("  - 'APIs': appendix/apis.md\n")
    
    # create single appendix/apis.md file for pdf generation
    markdown_filepath = markdownfolder + "appendix/apis.md"
    with open(markdown_filepath, 'wt') as fout:
        fout.write("\n# APIs\n\n")
        for header_filepath in sorted(header_files.keys()):
            filename = os.path.basename(header_filepath)
            filename_without_extension = filename_stem(header_filepath) # file name without 
            filetitle = header_files[header_filepath][0]

            description = header_files[header_filepath][1]

            if len(description) > 1:
                description = ": " + description

            header_description = api_description.replace("FILENAME", filename).replace("DESCRIPTION", description)

            subheader_title = api_subheader.replace("API_TITLE", filetitle).replace("API_LABEL", filename_without_extension)            
            with open(header_filepath, 'rt') as fin:
                    fout.write(subheader_title)
                    fout.write(header_description)
                    writeAPI(fout, fin, mk_codeidentation)
                

    references = functions.copy()
    references.update(typedefs)

    for function_name, url in references.items():
        print(function_name, url)

    # with open(indexfile, 'w') as fout:
    #     for function, reference in references.items():
    #         fout.write("[" + function + "](" + reference + ")\n")
            
    pickle.dump(references, open("references.p", "wb" ) )

if __name__ == "__main__":
   main(sys.argv[1:])
