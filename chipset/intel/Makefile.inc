#
# Makefile to download firmware and config files for Intel Wireless chipsets from Linux firmware tree
#
# Supported:
# - Intel Wireless 8260
# - Intel Wireless 8265

LINUX_FIRMWARE_URL = https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain/

INTEL_FILES = \
	ibt-11-5.ddc  \
	ibt-11-5.sfi  \
	ibt-12-16.ddc \
	ibt-12-16.sfi 

all-intel: $(INTEL_FILES)

%.sfi:
	@echo Downloading $@
	@curl -O -L $(LINUX_FIRMWARE_URL)/intel/$@

%.ddc:
	@echo Downloading $@
	@curl -O -L $(LINUX_FIRMWARE_URL)/intel/$@

clean-intel:
	rm -f $(INTEL_FILES)

