GITHUB_URL = "https://github.com/bluekitchen/btstack/tree/"
BTSTACK_FOLDER = ../../
INTRO_FOLDER = docs-intro/
MARKDOWN_FOLDER = docs-markdown/
MKDOCS_FOLDER = docs/
HTML_FOLDER = btstack/

all: html pdf

docs-markdown:
	# create new docs_markdown
	rm -rf docs-markdown
	cp -r  docs-template docs-markdown
	mkdir  docs-markdown/examples
	mkdir  docs-markdown/ports

	# create mkdocs-temp.yml
	./update_mkdocs_yml.sh
	
	# following should create files in docs-markdown

	# Use chipsets/readme as chipsets.md
	sed -e "s|../doc/manual/docs-template/||g" ../../chipset/README.md > docs-markdown/chipsets.md

	# create docs-markdown/appendix/apis.md
	# create docs-markdown/api_index.md
	# create references.p
	# create mkdocs.yml
	./markdown_create_apis.py -r ${BTSTACK_FOLDER} -g ${GITHUB_URL} -o ${MARKDOWN_FOLDER}

	# create docs-markdown/examples/examples.md
	./markdown_create_examples.py -r ${BTSTACK_FOLDER} -t ${INTRO_FOLDER} -o ${MARKDOWN_FOLDER}

	# create docs-markdown/ports/existing_ports.md
	./markdown_create_ports.py -r ${BTSTACK_FOLDER} -t ${INTRO_FOLDER} -o ${MARKDOWN_FOLDER}

	# create docs-markdown/gatt_clients.md and  docs-template/gatt_services.md
	./markdown_create_gatt_services_and_clients.py -r ${BTSTACK_FOLDER} -t ${INTRO_FOLDER} -o ${MARKDOWN_FOLDER}

docs: docs-markdown 
	# create new docs
	rm -rf docs
	cp -r  docs-markdown docs

	# docs-markdown -> docs
	./markdown_update_references.py  -i ${MARKDOWN_FOLDER} -o ${MKDOCS_FOLDER} 
	

html: docs
	# generate HTML into btstack folder
	mkdocs build --clean
	
	# post-process HTML using references.p 
	./html_postprocess_code_blocks.py -o ${HTML_FOLDER}

pdf: docs
	rm -rf latex
	mkdir -p latex
	cp -r docs/picts latex
	# create latex/btstack_gettingstartec.tex with version
	./update_getting_started.sh
	# create latex/btstack_generated.md -> latex/btstack_final.tex
	./markdown2tex.py
	cp docs/ports/*.jpg latex
	cd latex && pdflatex btstack_gettingstarted.tex && pdflatex btstack_gettingstarted.tex
	mv latex/btstack_gettingstarted.pdf btstack.pdf

preview: docs
	# race condition, open browser before starting MKdocs server
	open http://127.0.0.1:8010
	mkdocs serve -a localhost:8010

clean:
	rm -rf docs-markdown docs tmp btstack *.pdf latex/btstack_generated.* latex/btstack_final.tex mkdocs.yml
	rm -rf latex btstack help
	rm -rf references.p mkdocs-temp.yml

	
