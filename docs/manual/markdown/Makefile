all:
	./update_apis.py	
	./update_listings.py
	mkdocs build --clean
	echo "a.toctree-l4 { display: none; }" >> btstack/css/theme_extra.css
	# mkdir -p latex
	# cp -r docs/picts latex
	# cp ../picts/bklogo.pdf latex/picts
	# ./markdown2pdf.py
	# cp btstack_gettingstarted.tex latex
	# cd latex && pdflatex btstack_gettingstarted.tex && pdflatex btstack_gettingstarted.tex
	# mv latex/btstack_gettingstarted.pdf .
	# rm -rf latex

	
