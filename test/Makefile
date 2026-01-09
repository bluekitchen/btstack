LLVM_PROFDATA ?= llvm-profdata
LCOV ?= lcov

# Makefile to build and run all tests

SUBDIRS :=  \
	ad_parser \
	att_db \
	avdtp \
	avdtp_util \
	base64 \
	ble_client \
	btstack_link_key_db \
	btstack_util \
	btstack_memory \
	classic-oob-pairing \
	crypto \
	des_iterator \
	embedded \
	flash_tlv \
	gap \
	gatt-service-client \
	gatt_client \
	gatt_server \
	gatt_service_server \
	hfp \
	hid_parser \
	l2cap-cbm \
	l2cap-ecbm \
	le_device_db_tlv \
	linked_list \
	mesh \
	obex \
	ring_buffer \
	sdp \
	sdp_client \
	security_manager \
	tlv_posix

# not testing anything in source tree
#	maths \
#	pts \
# no unit tests
#	embedded \
#	gatt_server \
# mixes classic with ble
#    flash_tlv - contains btstack_linked_key_db tests
# hangs on buildbot (no output for 20 minutes -> abort)
#	security_manager_sc

SUBDIRS_BLE := \
	ad_parser \
	att_db \
	ble_client \
	btstack_memory \
	btstack_util \
	crypto \
	embedded \
	gap \
	gatt-service-client \
	gatt_client \
	gatt_server \
	gatt_service_server \
	hid_parser \
	l2cap-cbm \
	le_device_db_tlv \
	linked_list \
	ring_buffer \
	security_manager

# test fails

# not unit-tests
# avrcp \
# map_client \
# sbc \
#	gatt_server \

SUBDIRS_UNIQ := $(sort $(SUBDIRS) $(SUBDIRS_BLE))
test coverage-all clean: $(SUBDIRS_UNIQ)

test-ble: MAKECMDGOALS = test
test-ble: $(SUBDIRS_BLE)

coverage-ble: MAKECMDGOALS = coverage
coverage-ble: $(SUBDIRS_BLE)

$(SUBDIRS_UNIQ):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(SUBDIRS_UNIQ)
.SUFFIXES:

coverage%: export LLVM_PROFILE_FILE=coverage-%m-%p.profraw

coverage%:
	LCOV_INFO=$$(find */ -iname "*.info") &&\
	${LCOV} $$(printf -- '-a %s ' $$LCOV_INFO) --ignore-errors unused --ignore-errors inconsistent --rc branch_coverage=1 --exclude "/Applications/*" --exclude "/Library/*" --exclude "/usr/*" --exclude "*/test/*" -o $@.info
	genhtml $@.info --ignore-errors inconsistent --branch-coverage --demangle-cpp --config-file lcovrc --output-directory $@

coverage: coverage-all

clean:
	rm -f *.info
	rm -rf coverage-all coverage-ble

