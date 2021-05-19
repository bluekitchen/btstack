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
	# create latex/btstack_generated.md -> latex/btstack_final.tex
	./markdown2tex.py
	cp btstack_gettingstarted.tex latex
	cp docs/ports/*.jpg latex
	cd latex && pdflatex btstack_gettingstarted.tex && pdflatex btstack_gettingstarted.tex
	mv latex/btstack_gettingstarted.pdf btstack.pdf

preview: update_content html
	# race condition, open browser before starting MKdocs server
	open http://127.0.0.1:8000
	mkdocs serve

update_content:
	sed -e "s|../doc/manual/docs/||g" ../../chipset/README.md > docs/chipsets.md
	rm -rf tmp
	mkdir tmp
	# create mkdocs.yml
	./update_mkdocs_yml.sh
	# create docs/appendix/apis.md
	./update_apis.py	
	# create docs/examples/examples.md
	./update_listings.py
	# create docs/ports/existing_ports.md
	./ports2markdown.py
	# create docs/gatt_clients.md and  docs/gatt_services.md
	./update_gatt_services.py
	# re-create docs_final
	rm -rf docs_final
	cp -r docs docs_final

clean:
	rm -rf docs_final tmp btstack *.pdf latex/btstack_generated.* latex/btstack_final.tex mkdocs.yml
	rm -rf docs/appendix/apis.md api_index.md docs/examples/examples.md docs/chipsets.md
	rm -rf docs/gatt_clients.md docs/gatt_services.md
	rm -rf latex btstack help
	

	
