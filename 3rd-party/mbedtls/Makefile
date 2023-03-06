DESTDIR=/usr/local
PREFIX=mbedtls_
PERL ?= perl

.SILENT:

.PHONY: all no_test programs lib tests install uninstall clean test check lcov apidoc apidoc_clean

all: programs tests
	$(MAKE) post_build

no_test: programs

programs: lib mbedtls_test
	$(MAKE) -C programs

lib:
	$(MAKE) -C library

tests: lib mbedtls_test
	$(MAKE) -C tests

mbedtls_test:
	$(MAKE) -C tests mbedtls_test

library/%:
	$(MAKE) -C library $*
programs/%:
	$(MAKE) -C programs $*
tests/%:
	$(MAKE) -C tests $*

.PHONY: generated_files
generated_files: library/generated_files
generated_files: programs/generated_files
generated_files: tests/generated_files
generated_files: visualc_files

.PHONY: visualc_files
VISUALC_FILES = visualc/VS2013/mbedTLS.sln visualc/VS2013/mbedTLS.vcxproj
# TODO: $(app).vcxproj for each $(app) in programs/
visualc_files: $(VISUALC_FILES)

# Ensure that the .c files that generate_visualc_files.pl enumerates are
# present before it runs. It doesn't matter if the files aren't up-to-date,
# they just need to be present.
$(VISUALC_FILES): | library/generated_files
$(VISUALC_FILES): scripts/generate_visualc_files.pl
$(VISUALC_FILES): scripts/data_files/vs2013-app-template.vcxproj
$(VISUALC_FILES): scripts/data_files/vs2013-main-template.vcxproj
$(VISUALC_FILES): scripts/data_files/vs2013-sln-template.sln
# TODO: also the list of .c and .h source files, but not their content
$(VISUALC_FILES):
	echo "  Gen   $@ ..."
	$(PERL) scripts/generate_visualc_files.pl

ifndef WINDOWS
install: no_test
	mkdir -p $(DESTDIR)/include/mbedtls
	cp -rp include/mbedtls $(DESTDIR)/include
	mkdir -p $(DESTDIR)/include/psa
	cp -rp include/psa $(DESTDIR)/include

	mkdir -p $(DESTDIR)/lib
	cp -RP library/libmbedtls.*    $(DESTDIR)/lib
	cp -RP library/libmbedx509.*   $(DESTDIR)/lib
	cp -RP library/libmbedcrypto.* $(DESTDIR)/lib

	mkdir -p $(DESTDIR)/bin
	for p in programs/*/* ; do              \
	    if [ -x $$p ] && [ ! -d $$p ] ;     \
	    then                                \
	        f=$(PREFIX)`basename $$p` ;     \
	        cp $$p $(DESTDIR)/bin/$$f ;     \
	    fi                                  \
	done

uninstall:
	rm -rf $(DESTDIR)/include/mbedtls
	rm -rf $(DESTDIR)/include/psa
	rm -f $(DESTDIR)/lib/libmbedtls.*
	rm -f $(DESTDIR)/lib/libmbedx509.*
	rm -f $(DESTDIR)/lib/libmbedcrypto.*

	for p in programs/*/* ; do              \
	    if [ -x $$p ] && [ ! -d $$p ] ;     \
	    then                                \
	        f=$(PREFIX)`basename $$p` ;     \
	        rm -f $(DESTDIR)/bin/$$f ;      \
	    fi                                  \
	done
endif


WARNING_BORDER_LONG      =**********************************************************************************\n
CTR_DRBG_128_BIT_KEY_WARN_L1=****  WARNING!  MBEDTLS_CTR_DRBG_USE_128_BIT_KEY defined!                      ****\n
CTR_DRBG_128_BIT_KEY_WARN_L2=****  Using 128-bit keys for CTR_DRBG limits the security of generated         ****\n
CTR_DRBG_128_BIT_KEY_WARN_L3=****  keys and operations that use random values generated to 128-bit security ****\n

CTR_DRBG_128_BIT_KEY_WARNING=\n$(WARNING_BORDER_LONG)$(CTR_DRBG_128_BIT_KEY_WARN_L1)$(CTR_DRBG_128_BIT_KEY_WARN_L2)$(CTR_DRBG_128_BIT_KEY_WARN_L3)$(WARNING_BORDER_LONG)

# Post build steps
post_build:
ifndef WINDOWS

	# If 128-bit keys are configured for CTR_DRBG, display an appropriate warning
	-scripts/config.py get MBEDTLS_CTR_DRBG_USE_128_BIT_KEY && ([ $$? -eq 0 ]) && \
	    echo '$(CTR_DRBG_128_BIT_KEY_WARNING)'

endif

clean: clean_more_on_top
	$(MAKE) -C library clean
	$(MAKE) -C programs clean
	$(MAKE) -C tests clean

clean_more_on_top:
ifndef WINDOWS
	find . \( -name \*.gcno -o -name \*.gcda -o -name \*.info \) -exec rm {} +
endif

neat: clean_more_on_top
	$(MAKE) -C library neat
	$(MAKE) -C programs neat
	$(MAKE) -C tests neat
ifndef WINDOWS
	rm -f visualc/VS2013/*.vcxproj visualc/VS2013/mbedTLS.sln
else
	if exist visualc\VS2013\*.vcxproj del /Q /F visualc\VS2013\*.vcxproj
	if exist visualc\VS2013\mbedTLS.sln del /Q /F visualc\VS2013\mbedTLS.sln
endif

check: lib tests
	$(MAKE) -C tests check

test: check

ifndef WINDOWS
# For coverage testing:
# 1. Build with:
#         make CFLAGS='--coverage -g3 -O0' LDFLAGS='--coverage'
# 2. Run the relevant tests for the part of the code you're interested in.
#    For the reference coverage measurement, see
#    tests/scripts/basic-build-test.sh
# 3. Run scripts/lcov.sh to generate an HTML report.
lcov:
	scripts/lcov.sh

apidoc:
	mkdir -p apidoc
	cd doxygen && doxygen mbedtls.doxyfile

apidoc_clean:
	rm -rf apidoc
endif

## Editor navigation files
C_SOURCE_FILES = $(wildcard \
	3rdparty/*/include/*/*.h 3rdparty/*/include/*/*/*.h 3rdparty/*/include/*/*/*/*.h \
	3rdparty/*/*.c 3rdparty/*/*/*.c 3rdparty/*/*/*/*.c 3rdparty/*/*/*/*/*.c \
	include/*/*.h \
	library/*.[hc] \
	programs/*/*.[hc] \
	tests/include/*/*.h tests/include/*/*/*.h \
	tests/src/*.c tests/src/*/*.c \
	tests/suites/*.function \
)
# Exuberant-ctags invocation. Other ctags implementations may require different options.
CTAGS = ctags --langmap=c:+.h.function --line-directives=no -o
tags: $(C_SOURCE_FILES)
	$(CTAGS) $@ $(C_SOURCE_FILES)
TAGS: $(C_SOURCE_FILES)
	etags --no-line-directive -o $@ $(C_SOURCE_FILES)
global: GPATH GRTAGS GSYMS GTAGS
GPATH GRTAGS GSYMS GTAGS: $(C_SOURCE_FILES)
	ls $(C_SOURCE_FILES) | gtags -f - --gtagsconf .globalrc
cscope: cscope.in.out cscope.po.out cscope.out
cscope.in.out cscope.po.out cscope.out: $(C_SOURCE_FILES)
	cscope -bq -u -Iinclude -Ilibrary $(patsubst %,-I%,$(wildcard 3rdparty/*/include)) -Itests/include $(C_SOURCE_FILES)
.PHONY: cscope global
