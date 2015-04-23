pdf: clean
	python update_listings.py 
	pdflatex btstack_gettingstarted.tex 
	pdflatex btstack_gettingstarted.tex 
	open btstack_gettingstarted.pdf

example:
	python update_listings.py 
	subl examples.tex
	pdflatex examples.tex  
	open examples.pdf
	
clean:
	rm -f $ *.pdf *.log *.aux *.gz *.toc *.out
