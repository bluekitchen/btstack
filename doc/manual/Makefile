all: update_docs_and_apis html pdf

html:
	rm -rf docs_tmp
	rm -rf docs_final
	cp -r docs docs_tmp
	cp -r docs docs_final
	./markdown2mkdocs.py
	rm -rf docs_tmp
	mkdocs build --clean
	./mkdocs2html.py

pdf:
	mkdir -p latex
	cp -r docs/picts latex
	./markdown2pdf.py
	cp btstack_gettingstarted.tex latex
	cd latex && pdflatex btstack_gettingstarted.tex && pdflatex btstack_gettingstarted.tex
	mv latex/btstack_gettingstarted.pdf btstack.pdf
	rm -rf latex tmp


preview: update_docs_and_apis html
	# race condition, open browser before startnig MKdocs
	open http://127.0.0.1:8000
	mkdocs serve

update_docs_and_apis:
	sed -e "s|../doc/manual/docs/||g" ../../chipset/README.md > docs/chipsets.md
	rm -rf tmp
	mkdir tmp
	./update_apis.py	
	./update_listings.py

clean:
	rm -rf docs_final tmp btstack *.pdf latex/btstack_generated.* latex/btstack_final.tex
	rm -rf docs/appendix/apis.md docs/appendix/index.md docs/examples/examples.md docs/chipsets.md
	rm -rf docs_tmp latex btstack help
	

	
