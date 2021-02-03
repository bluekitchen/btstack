
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
	crypto \
	des_iterator \
	flash_tlv \
	gap \
	gatt_client \
	gatt_server \
	gatt_service \
	hfp \
	hid_parser \
	le_device_db_tlv \
	linked_list \
	map_test \
	mesh \
	obex \
	pts \
	ring_buffer \
	sdp \
	sdp_client \
	security_manager \
	tlv_posix \
	embedded \

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
	gatt_client \
	gatt_server \
	gatt_service \
	hid_parser \
	le_device_db_tlv \
	linked_list \
	ring_buffer \
    gatt_server \
    security_manager \

# test fails

# not unit-tests
# avrcp \
# map_client \
# sbc \
#	gatt_server \

.PHONY: coverage coverage-sm-sc.info coverage-pts.info

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


coverage-pts.info:
	# download pts coverage and fix paths
	curl https://bluekitchen-gmbh.com/btstack/develop/coverage-pts.info | sed "s|/Users/mringwal/buildbot-worker/auto-pts/btstack|`realpath ..`|" > coverage-pts.info

coverage-sm-sc.info:
	# download pts coverage and fix paths
	curl https://bluekitchen-gmbh.com/btstack/develop/coverage-sm-sc.info | sed "s|/Users/mringwal/buildbot-worker/test-develop-sm-sc/build|`realpath ..`|" > coverage-sm-sc.info


coverage-unit.info: subdirs-coverage
	# delete trace data
	find . -name "*.gcda" -type f -delete
	# run tests
	$(MAKE) subdirs-coverage
	# collect traces
	lcov --capture --rc lcov_branch_coverage=1 --directory . --exclude "/Applications/*" --exclude "/Library/*" --exclude "/usr/*" --exclude "*/test/*" --output-file coverage-unit.info
	# demangle
	python3 coverage_demangle.py coverage-unit.info

coverage-unit-ble.info: subdirs-coverage-ble
	# delete trace data
	find . -name "*.gcda" -type f -delete
	# run tests
	$(MAKE) subdirs-coverage-ble
	# collect traces
	lcov --capture --rc lcov_branch_coverage=1 --directory . --exclude "/Applications/*" --exclude "/Library/*" --exclude "/usr/*" --exclude "*/test/*" --output-file coverage-unit-ble.info
	# demangle
	python3 coverage_demangle.py coverage-unit-ble.info


coverage-ble.info: coverage-pts.info coverage-sm-sc.info coverage-unit-ble.info
	# combine unit-ble and pts
	lcov --rc lcov_branch_coverage=1 -a coverage-pts.info -a coverage-sm-sc.info -a coverage-unit-ble.info --output-file coverage-ble.info


coverage-bat.info: coverage-ble.info
	# create bat subset from ble subset
	./coverage_subset_bat.py coverage-ble.info coverage-bat.info


coverage.info: coverage-pts.info coverage-sm-sc.info coverage-unit.info
	# combine unit-ble and pts
	lcov --rc lcov_branch_coverage=1 -a coverage-pts.info -a coverage-sm-sc.info -a coverage-unit.info --output-file coverage.info


coverage-all: coverage.info
	# generate html output
	genhtml coverage.info     --branch-coverage --demangle-cpp --config-file lcovrc --output-directory coverage

coverage-bat: coverage-bat.info
	# generate html output
	genhtml coverage-bat.info --branch-coverage --demangle-cpp --config-file lcovrc --output-directory coverage-bat

coverage-ble: coverage-ble.info
	# generate html output
	genhtml coverage-ble.info --branch-coverage --demangle-cpp --config-file lcovrc --output-directory coverage-ble

coverage-unit-ble: coverage-unit-ble.info
	# generate html output
	genhtml coverage-unit-ble.info --branch-coverage --demangle-cpp --config-file lcovrc --output-directory coverage-unit-ble

coverage-sm-sc: coverage-sm-sc.info
	# generate html output
	genhtml coverage-sm-sc.info --branch-coverage --demangle-cpp --config-file lcovrc --output-directory coverage-sm-sc

coverage-pts: coverage-pts.info
	# generate html output
	genhtml coverage-pts.info --branch-coverage --demangle-cpp --config-file lcovrc --output-directory coverage-pts

coverage: coverage-all coverage-ble coverage-bat

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
