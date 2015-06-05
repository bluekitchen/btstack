all:
	./update_apis.py	
	./update_listings.py
	mkdocs build --clean
	echo "a.toctree-l4 { display: none; }" >> btstack/css/theme_extra.css
