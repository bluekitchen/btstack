
# Makefile to build and run all tests

SUBDIRS =  \
	att_db \
	ble_client \
	des_iterator \
	gatt_client \
	linked_list \
	remote_device_db \
	sdp_client \
	security_manager \

#	hfp \


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


