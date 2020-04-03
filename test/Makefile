
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

coverage:
	# delete trace data
	rm -f coverage.info
	find . -name "*.gcda" -type f -delete
	# collect all files
	# lcov --capture --initial --directory .  --output-file coverage.info
	# run tests
	make test
	# collect traces
	lcov --capture --rc lcov_branch_coverage=1 --directory . --exclude "/Applications/*" --exclude "/Library/*" --exclude "/usr/*" --exclude "*/3rd-party/tinydir/*" --exclude "*/platform/posix/*" --exclude "*/test/*" --output-file coverage.info
	# generate html output
	genhtml coverage.info --branch-coverage --output-directory coverage-html

coverage-freertos-ble:
	./coverage_filter.py src/mesh src/classic
