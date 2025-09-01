.phony: examples
PYTHON ?= python3
all: examples

examples:
	$(PYTHON) scripts/create_examples.py

clean:
	$(PYTHON) scripts/delete_examples.py
	@rm -f example/Makefile
	@rm -fR example/bin
