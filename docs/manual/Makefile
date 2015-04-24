pdf: clean
	./update_listings.py 
	pdflatex btstack_gettingstarted.tex 
	pdflatex btstack_gettingstarted.tex 
	open btstack_gettingstarted.pdf

example:
	./update_listings.py -s
	subl examples.tex
	pdflatex examples.tex  
	open examples.pdf
	
clean:
	rm -f $ *.pdf *.log *.aux *.gz *.toc *.out
