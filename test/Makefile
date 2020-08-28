
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
	embedded \
	flash_tlv \
	gatt_client \
	gatt_server \
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

# not testing anything in source tree
#	maths \

# test fails

# not unit-tests
# avrcp \
# map_client \
# sbc \
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

test-coverage:
	# delete trace data
	rm -f coverage.info
	find . -name "*.gcda" -type f -delete
	# collect all files
	# lcov --capture --initial --directory .  --output-file coverage.info
	# run tests
	make test
	# collect traces
	lcov --capture --rc lcov_branch_coverage=1 --directory . --exclude "/Applications/*" --exclude "/Library/*" --exclude "/usr/*" --exclude "*/test/*" --output-file coverage-unit.info

coverage: test-coverage

	# TODO: download pts coverage
	# ...
	# combine unit and pts
	# lcov --rc lcov_branch_coverage=1  -a coverage-pts.info -a coverage-unit.info --output-file coverage.info
	cp coverage-unit.info coverage.info

	# create bat subset
	./coverage_subset_bat.py coverage.info coverage-bat.info

	# generate html output
	genhtml coverage-unit.info --branch-coverage --output-directory coverage-unit
	genhtml coverage-bat.info  --branch-coverage --output-directory coverage-bat
	genhtml coverage.info      --branch-coverage --output-directory coverage

coverage-freertos-ble:
	./coverage_filter.py src/mesh src/classic
