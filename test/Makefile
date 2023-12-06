
# Makefile to build and run all tests

SUBDIRS =  \
	ad_parser \
	att_db \
	avdtp \
	avdtp_util \
	base64 \
	ble_client \
	btstack_link_key_db \
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
	tlv_posix \

# not testing anything in source tree
#	maths \
# no unit tests
#	embedded \
#	gatt_server \
# mixes classic with ble
#    flash_tlv - contains btstack_linked_key_db tests
# hangs on buildbot (no output for 20 minutes -> abort)
#	security_manager_sc

SUBDIRS_BLE = \
	ad_parser \
	att_db \
	ble_client \
	btstack_memory \
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
	security_manager \

# test fails

# not unit-tests
# avrcp \
# map_client \
# sbc \
#	gatt_server \

.PHONY: coverage coverage-sm-sc.info

subdirs:
	echo Building all tests
	@set -e; \
	for dir in $(SUBDIRS); do \
	  $(MAKE) -C $$dir; \
	done

clean:
	echo Clean all test
	@set -e; \
	for dir in $(SUBDIRS); do \
	  $(MAKE) -C $$dir clean; \
	done
	rm -f *.info
	rm -rf coverage coverage-unit-ble

subdirs-coverage:
	echo Run all tests for coverage
	@set -e; \
	for dir in $(SUBDIRS); do \
	  $(MAKE) -C $$dir coverage; \
	done

subdirs-coverage-ble:
	echo Run all BLE tests for coverage
	@set -e; \
	for dir in $(SUBDIRS_BLE); do \
	  $(MAKE) -C $$dir coverage; \
	done


coverage-unit.info: subdirs-coverage
	# delete trace data
	find . -name "*.gcda" -type f -delete
	# run tests
	$(MAKE) subdirs-coverage
	# collect traces
	lcov --capture --ignore-errors inconsistent --ignore-errors unused --rc lcov_branch_coverage=1 --directory . --exclude "/Applications/*" --exclude "/Library/*" --exclude "/usr/*" --exclude "*/test/*" --output-file coverage-unit.info

coverage-unit-ble.info: subdirs-coverage-ble
	# delete trace data
	find . -name "*.gcda" -type f -delete
	# run tests
	$(MAKE) subdirs-coverage-ble
	# collect traces
	lcov --capture --ignore-errors inconsistent --ignore-errors unused --rc lcov_branch_coverage=1 --directory . --exclude "/Applications/*" --exclude "/Library/*" --exclude "/usr/*" --exclude "*/test/*" --output-file coverage-unit-ble.info


coverage-all: coverage-unit.info
	# generate html output
	genhtml coverage-unit.info --branch-coverage --demangle-cpp --config-file lcovrc --output-directory coverage

coverage-unit-ble: coverage-unit-ble.info
	# generate html output
	genhtml coverage-unit-ble.info --branch-coverage --demangle-cpp --config-file lcovrc --output-directory coverage-unit-ble


coverage: coverage-all coverage-unit-ble

test:
	echo Run all tests with ASAN
	@set -e; \
	for dir in $(SUBDIRS); do \
	  $(MAKE) -C $$dir test; \
	done

test-ble:
	echo Run all BLE tests with ASAN
	@set -e; \
	for dir in $(SUBDIRS_BLE); do \
	  $(MAKE) -C $$dir test; \
	done
