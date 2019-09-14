
# Makefile to build and run all tests

SUBDIRS =  \
	att_db \
	avdtp \
	avrcp \
	base64 \
	ble_client \
	btstack_link_key_db \
	crypto \
	des_iterator \
	gatt_client \
	hfp \
	hid_parser \
	linked_list \
	maths \
	obex \
	ring_buffer \
	sdp_client \
	security_manager \
	tlv_posix \


# test fails
# flash_tlv \
# map_test \
# mesh \
# sdp \

# not unit-tests
# map_client \
# sbc \

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


