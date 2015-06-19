all:
	./update_apis.py	
	./update_listings.py
	rm -rf docs_tmp
	rm -rf docs_final
	cp -r docs docs_tmp
	cp -r docs docs_final
	./markdown2mkdocs.py
	rm -rf docs_tmp
	mkdocs build --clean
	mkdir -p latex
	cp -r docs/picts latex
	cp ../picts/bklogo.pdf latex/picts
	./markdown2pdf.py
	cp btstack_gettingstarted.tex latex
	cd latex && pdflatex btstack_gettingstarted.tex && pdflatex btstack_gettingstarted.tex
	mv latex/btstack_gettingstarted.pdf btstack.pdf
	rm -rf latex

	
