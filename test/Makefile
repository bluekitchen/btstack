
# Makefile to build and run all tests

SUBDIRS =  \
	att_db \
	avdtp \
	avdtp_util \
	base64 \
	ble_client \
	btstack_link_key_db \
	crypto \
	des_iterator \
	flash_tlv \
	gatt_client \
	gap \
	hfp \
	hid_parser \
	linked_list \
	map_test \
	mesh \
	obex \
	ring_buffer \
	sdp \
	sdp_client \
	security_manager \
	tlv_posix \
	security_manager_sc \

# not testing anything in source tree
#	maths \
# no unit tests
#	embedded \
#	gatt_server \

# flash_tlv also contains btstack_linked_key_db tests

SUBDIRS_BLE = \
	att_db \
	ble_client \
	crypto \
	gatt_client \
	gap \
	hid_parser \
	linked_list \
	ring_buffer \
	security_manager \
	security_manager_sc \
#	tlv_posix \

# test fails

# not unit-tests
# avrcp \
# map_client \
# sbc \
#	gatt_server \

.PHONY: coverage

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

test:
	echo Run all test
	@set -e; \
	for dir in $(SUBDIRS); do \
	  $(MAKE) -C $$dir test; \
	done

test-ble:
	echo Run all BLE tests
	@set -e; \
	for dir in $(SUBDIRS_BLE); do \
	  $(MAKE) -C $$dir test; \
	done

test-coverage-ble: subdirs
	# delete trace data
	find . -name "*.gcda" -type f -delete
	# run tests
	$(MAKE) test-ble
	# collect traces
	lcov --capture --rc lcov_branch_coverage=1 --directory . --exclude "/Applications/*" --exclude "/Library/*" --exclude "/usr/*" --exclude "*/test/*" --output-file coverage-unit-ble.info
	# demangle
	python3 coverage_demangle.py coverage-unit-ble.info

test-coverage: subdirs
	# delete trace data
	find . -name "*.gcda" -type f -delete
	# run tests
	$(MAKE) test
	# collect traces
	lcov --capture --rc lcov_branch_coverage=1 --directory . --exclude "/Applications/*" --exclude "/Library/*" --exclude "/usr/*" --exclude "*/test/*" --output-file coverage-unit.info
	# demangle
	python3 coverage_demangle.py coverage-unit.info

coverage: test-coverage test-coverage-ble
	# download pts coverage and fix paths
	curl https://bluekitchen-gmbh.com/btstack/develop/coverage-pts.info | sed 's|buildbot-worker/auto-pts/btstack|buildbot-worker/test-develop/build|' > coverage-pts.info

	# combine unit[-ble] and pts
	lcov --rc lcov_branch_coverage=1 -a coverage-pts.info -a coverage-unit.info     --output-file coverage.info
	lcov --rc lcov_branch_coverage=1 -a coverage-pts.info -a coverage-unit-ble.info --output-file coverage-ble.info

	# create bat subset from ble subset
	./coverage_subset_bat.py coverage-ble.info      coverage-bat.info
	./coverage_subset_bat.py coverage-unit-ble.info coverage-unit-bat.info

	./coverage_subset_bat.py coverage-pts.info coverage-pts-bat.info

	# generate html output
	genhtml coverage-unit.info     --branch-coverage --demangle-cpp --output-directory coverage-unit
	genhtml coverage-unit-bat.info --branch-coverage --demangle-cpp --output-directory coverage-unit-bat
	genhtml coverage-bat.info      --branch-coverage --demangle-cpp --output-directory coverage-bat
	genhtml coverage.info          --branch-coverage --demangle-cpp --output-directory coverage
	genhtml coverage-pts-bat.info  --branch-coverage --demangle-cpp --output-directory coverage-pts-bat
