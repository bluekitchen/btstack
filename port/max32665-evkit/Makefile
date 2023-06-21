.phony: examples
all: examples

examples: 
	scripts/create_examples.py

clean:
	scripts/delete_examples.py
	@rm -rf example/Makefile
	@echo "Deleting CC2564B Init Script in src folder"
	@rm -rf src/cc256x* bluetooth_init*
