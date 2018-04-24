.phony: examples
all: examples

examples:
	scripts/create_examples.py

clean:
	scripts/delete_examples.py
	@rm -rf example/Makefile

-include ../../chipset/atwilc3000/Makefile.inc
