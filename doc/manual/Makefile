GITHUB_URL = "https://github.com/bluekitchen/btstack/tree/"
BTSTACK_FOLDER = ../../
INTRO_FOLDER = docs-intro/
MARKDOWN_FOLDER = docs-markdown/
MKDOCS_FOLDER = docs/
HTML_FOLDER = btstack/
LATEX_FOLDER = latex/

all: html pdf

docs-markdown:
	# create new docs_markdown
	rm -rf ${MARKDOWN_FOLDER}
	cp -r  docs-template ${MARKDOWN_FOLDER}
	mkdir  ${MARKDOWN_FOLDER}/examples
	mkdir  ${MARKDOWN_FOLDER}/ports

	# create mkdocs-temp.yml
	./update_mkdocs_yml.sh
	
	# following should create files in docs-markdown

	# Use chipsets/readme as chipsets.md
	sed -e "s|../doc/manual/docs-template/||g" ../../chipset/README.md > ${MARKDOWN_FOLDER}/chipsets.md

	# create docs-markdown/appendix/apis.md
	# create references.p
	# create mkdocs.yml
	# create mkdocs-latex.yml
	./markdown_create_apis.py -r ${BTSTACK_FOLDER} -g ${GITHUB_URL} -o ${MARKDOWN_FOLDER}

	# create docs-markdown/examples/examples.md
	./markdown_create_examples.py -r ${BTSTACK_FOLDER} -t ${INTRO_FOLDER} -o ${MARKDOWN_FOLDER}

	# create docs-markdown/ports/existing_ports.md
	./markdown_create_ports.py -r ${BTSTACK_FOLDER} -t ${INTRO_FOLDER} -o ${MARKDOWN_FOLDER}

	# create docs-markdown/gatt_clients.md and  docs-template/gatt_services.md
	./markdown_create_gatt_services_and_clients.py -r ${BTSTACK_FOLDER} -t ${INTRO_FOLDER} -o ${MARKDOWN_FOLDER}

docs: docs-markdown 
	# create new docs
	rm -rf ${MKDOCS_FOLDER}
	cp -r ${MARKDOWN_FOLDER} ${MKDOCS_FOLDER}

	# docs-markdown -> docs
	./markdown_update_references.py  -i ${MARKDOWN_FOLDER} -o ${MKDOCS_FOLDER} 
	

html: docs
	# generate HTML into btstack folder
	mkdocs build --clean
	
	# post-process HTML using references.p 
	./html_postprocess_code_blocks.py -o ${HTML_FOLDER}

pdf: docs
	rm -rf ${LATEX_FOLDER}
	mkdir -p ${LATEX_FOLDER}
	cp -r ${MKDOCS_FOLDER}/picts ${LATEX_FOLDER}

	cp mkdocs-latex.yml mkdocs.yml
	# create latex/btstack_gettingstartec.tex with version
	./update_getting_started.sh ${LATEX_FOLDER}
	
	# create latex/btstack_generated.md -> latex/btstack_final.tex
	./markdown2tex.py  -i ${MKDOCS_FOLDER} -o ${LATEX_FOLDER}
	
	cp ${MKDOCS_FOLDER}/ports/*.jpg ${LATEX_FOLDER}
	cd ${LATEX_FOLDER} && pdflatex btstack_gettingstarted.tex && pdflatex btstack_gettingstarted.tex
	mv ${LATEX_FOLDER}/btstack_gettingstarted.pdf btstack.pdf

preview: docs
	# race condition, open browser before starting MKdocs server
	open http://127.0.0.1:8010
	mkdocs serve -a localhost:8010

clean:
	rm -rf ${MARKDOWN_FOLDER} ${LATEX_FOLDER} ${HTML_FOLDER} ${MKDOCS_FOLDER}
	rm -f *.pdf 
	rm -f references.p 
	rm -f mkdocs.yml mkdocs-latex.yml mkdocs-temp.yml 

	
