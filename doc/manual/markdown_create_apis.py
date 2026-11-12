#!/usr/bin/env python3
import os, sys, getopt, re, pickle
import subprocess
import api_reference

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

# Groups are ordered for the navigation. Comment out a group to omit its APIs.
API_GROUPS = (
    'Core',
    'Bluetooth Low Energy',
    'Bluetooth Classic',
    'Audio and codecs',
    'Platform and diagnostics',
    'Other APIs',
    # 'Mesh',
    # 'LE Audio',
)
CORE_HEADERS = frozenset("""ad_parser btstack_base64_decoder btstack_base64_encoder
btstack_chunk_buffer btstack_crypto btstack_event btstack_fsm btstack_hid
btstack_hid_parser btstack_hsm btstack_linked_list btstack_linked_queue
btstack_ltv_builder btstack_memory btstack_memory_pool btstack_ring_buffer
btstack_run_loop btstack_run_loop_base btstack_tlv btstack_tlv_builder
btstack_tlv_none btstack_util btstack_version gap hci hci_cmd hci_event_builder
l2cap""".split())
AUDIO_HEADERS = frozenset("""btstack_audio btstack_audio_generator btstack_audio_player
btstack_audio_song btstack_lc3 btstack_lc3_google btstack_lc3plus_fraunhofer
btstack_resample btstack_sample_rate_compensation""".split())
CLASSIC_AUDIO_HEADERS = frozenset("""btstack_cvsd_plc btstack_sbc
btstack_sbc_bluedroid btstack_sbc_plc hfp_codec hfp_msbc""".split())
PLATFORM_HEADERS = frozenset("""btstack_chipset btstack_control btstack_debug
btstack_em9304_spi btstack_network btstack_sco_transport btstack_slip
btstack_stdin btstack_uart btstack_uart_block btstack_uart_slip_wrapper
hci_dump hci_dump_buffered hci_dump_dispatch hci_transport
hci_transport_em9304_spi hci_transport_h4 hci_transport_h5 hci_transport_usb""".split())
MESH_PROVISIONING = frozenset("""pb_adv pb_gatt provisioning provisioning_device
provisioning_provisioner mesh_provisioning_service_server""".split())
MESH_MODELS = frozenset("""mesh_configuration_client mesh_configuration_server
mesh_foundation mesh_generic_model mesh_generic_default_transition_time_client
mesh_generic_default_transition_time_server mesh_generic_level_client
mesh_generic_level_server mesh_generic_on_off_client mesh_generic_on_off_server
mesh_health_server""".split())
MESH_NETWORK = frozenset("""adv_bearer beacon gatt_bearer mesh_access mesh_crypto
mesh_iv_index_seq_number mesh_keys mesh_lower_transport mesh_network mesh_peer
mesh_proxy mesh_proxy_service_server mesh_upper_transport mesh_virtual_addresses""".split())
MESH_SUBGROUPS = ('Provisioning', 'Models', 'Network and transport')


def api_group(relative_path):
    """Return the navigation group and optional subgroup for a src-relative path."""
    path = relative_path.replace('\\', '/')
    stem = filename_stem(path)
    directory = path.split('/')[0] if '/' in path else ''
    if directory == 'ble':
        return ('Bluetooth Low Energy', 'GATT services and clients' if path.startswith('ble/gatt-service/') else None)
    if directory == 'classic':
        return ('Audio and codecs' if stem in CLASSIC_AUDIO_HEADERS else 'Bluetooth Classic', None)
    if directory == 'le-audio':
        return ('LE Audio', None)
    if directory == 'mesh':
        for subgroup, names in (('Provisioning', MESH_PROVISIONING), ('Models', MESH_MODELS),
                                ('Network and transport', MESH_NETWORK)):
            if stem in names:
                return ('Mesh', subgroup)
        return ('Mesh', None)
    if not directory:
        for group, names in (('Core', CORE_HEADERS), ('Audio and codecs', AUDIO_HEADERS),
                             ('Platform and diagnostics', PLATFORM_HEADERS)):
            if stem in names:
                return (group, None)
    return ('Other APIs', None)


def grouped_api_headers(headers, srcfolder):
    """Group and sort (path, title, description) entries by displayed title."""
    groups = {group: {} for group in API_GROUPS}
    for path, (title, description) in headers.items():
        group, subgroup = api_group(os.path.relpath(path, srcfolder))
        if group in groups:
            groups[group].setdefault(subgroup, []).append((path, title, description))
    for subgroups in groups.values():
        for entries in subgroups.values():
            entries.sort(key=lambda item: (item[1].casefold(), item[0]))
    return groups


def ordered_subgroups(group, subgroups):
    names = [None]
    if group == 'Mesh':
        names += list(MESH_SUBGROUPS)
    elif group == 'Bluetooth Low Energy':
        names += ['GATT services and clients']
    return [name for name in names if name in subgroups]


def write_api_navigation(fout, groups, indentation):
    """Write grouped MkDocs navigation using the indentation of the APIs entry."""
    for group, subgroups in groups.items():
        if not subgroups:
            continue
        fout.write(indentation + "- '" + group.replace("'", "''") + "':\n")
        for subgroup in ordered_subgroups(group, subgroups):
            entry_indent = indentation + '  '
            if subgroup:
                fout.write(entry_indent + "- '" + subgroup.replace("'", "''") + "':\n")
                entry_indent += '  '
            for path, title, _ in subgroups[subgroup]:
                fout.write(entry_indent + "- '" + title.replace("'", "''") + "': appendix/" + filename_stem(path) + ".md\n")


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
                fin.seek(0)
                has_api = "API_START" in fin.read()

            if header_title:
                header_files[header_filepath] = [header_title, header_desc]
            elif has_api:
                title = os.path.splitext(f)[0].replace('_', ' ')
                header_files[header_filepath] = [title, ""]
            else:
                print("No @title or API_START flag found. Skip %s" % header_filepath)

    # Include the generated event getter even if an older header has no title.
    event_header = btstack_srcfolder + "btstack_event.h"
    header_files.setdefault(event_header, ["Event Getters", "Generated accessors for event packets."])
    groups = grouped_api_headers(header_files, btstack_srcfolder)

    # Generate pages and references only for enabled groups.
    enabled_paths = sorted(path for subgroups in groups.values()
                           for entries in subgroups.values() for path, _, _ in entries)
    for header_filepath in enabled_paths:
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
                api_reference.render_header(fout, header_filepath, githuburl + "/" + filename, 2,
                                                group_events=filename_without_extension == "btstack_event")

        if filename_without_extension == 'btstack_event':
            continue

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

                indentation = re.match(r'^(\s*)', line).group(1) + '  '
                write_api_navigation(fout, groups, indentation)

    
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
        for group, subgroups in groups.items():
            if not subgroups:
                continue
            # The event getter reference is available only in HTML.
            if not any(filename_stem(path) != 'btstack_event'
                       for entries in subgroups.values() for path, _, _ in entries):
                continue
            fout.write("\n## " + group + "\n\n")
            for subgroup in ordered_subgroups(group, subgroups):
                entries = [entry for entry in subgroups[subgroup]
                           if filename_stem(entry[0]) != 'btstack_event']
                if not entries:
                    continue
                if subgroup:
                    fout.write("\n### " + subgroup + "\n\n")
                level = 4 if subgroup else 3
                for header_filepath, filetitle, description in entries:
                    filename = os.path.basename(header_filepath)
                    stem = filename_stem(header_filepath)
                    header_description = api_description.replace("FILENAME", filename).replace(
                        "DESCRIPTION", ": " + description if len(description) > 1 else "")
                    fout.write("#" * level + " " + filetitle + " API {#sec:" + stem + "_api}\n\n")
                    fout.write(header_description)
                    relative_path = os.path.relpath(header_filepath, btstackfolder)
                    api_reference.render_header(fout, header_filepath, githuburl + "/" + relative_path, level + 1)

                

    references = functions.copy()
    references.update(typedefs)

    for function_name, url in references.items():
        print(function_name, url)

    # with open(indexfile, 'w') as fout:
    #     for function, reference in references.items():
    #         fout.write("[" + function + "](" + reference + ")\n")
            
    with open("references.p", "wb") as references_file:
        pickle.dump(references, references_file)

if __name__ == "__main__":
   main(sys.argv[1:])
