.phony: examples
all: examples

examples: 
	scripts/create_examples.py

clean:
	scripts/delete_examples.py
	@rm -f example/Makefile
	@rm -fR example/bin
