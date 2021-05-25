all: update_content html pdf

html: 
	# docs -> docs_final
	./markdown2mkdocs.py
	# generate HTML
	mkdocs build --clean
	# post-process HTML
	./mkdocs2html.py

pdf:
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

preview: update_content html
	# race condition, open browser before starting MKdocs server
	open http://127.0.0.1:8010
	mkdocs serve -a localhost:8010

update_content:
	sed -e "s|../doc/manual/docs-template/||g" ../../chipset/README.md > docs-template/chipsets.md
	rm -rf tmp
	mkdir tmp
	# create mkdocs.yml
	./update_mkdocs_yml.sh
	# create docs-template/appendix/apis.md
	./update_apis.py	
	# create docs-template/examples/examples.md
	./update_listings.py
	# create docs-template/ports/existing_ports.md
	./ports2markdown.py
	# create docs-template/gatt_clients.md and  docs-template/gatt_services.md
	./update_gatt_services.py
	# re-create docs_final
	rm -rf docs
	cp -r docs-template docs

clean:
	rm -rf docs tmp btstack *.pdf latex/btstack_generated.* latex/btstack_final.tex mkdocs.yml
	rm -rf docs-template/appendix/apis.md docs-template/examples/examples.md docs-template/chipsets.md
	rm -rf docs-template/gatt_clients.md docs-template/gatt_services.md 
	rm -rf api_index.md
	rm -rf latex btstack help
	

	
