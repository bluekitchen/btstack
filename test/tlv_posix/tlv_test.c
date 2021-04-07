
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_tlv.h"
#include "btstack_tlv_posix.h"
#include "hci_dump.h"
#include "hci_dump_posix_fs.h"
#include "btstack_util.h"
#include "btstack_config.h"
#include "btstack_debug.h"
#include <unistd.h>

#define TEST_DB "/tmp/test.tlv"

#define TAG(a,b,c,d) ( ((a)<<24) | ((b)<<16) | ((c)<<8) | (d) )

/// TLV
TEST_GROUP(BSTACK_TLV){
	const btstack_tlv_t * btstack_tlv_impl;
	btstack_tlv_posix_t   btstack_tlv_context;
    void setup(void){
    	log_info("setup");
    	// delete old file
    	unlink(TEST_DB);
    	// open db
		btstack_tlv_impl = btstack_tlv_posix_init_instance(&btstack_tlv_context, TEST_DB);
    }
    void reopen_db(void){
    	log_info("reopen");
    	// close file 
    	fclose(btstack_tlv_context.file);
    	// reopen
        btstack_tlv_posix_deinit(&btstack_tlv_context);
		btstack_tlv_impl = btstack_tlv_posix_init_instance(&btstack_tlv_context, TEST_DB);
    }
    void teardown(void){
    	log_info("teardown");
    	// close file
    	fclose(btstack_tlv_context.file);
        btstack_tlv_posix_deinit(&btstack_tlv_context);
    }
};

TEST(BSTACK_TLV, TestMissingTag){
	uint32_t tag = TAG('a','b','c','d');
	int size = btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, NULL, 0);
	CHECK_EQUAL(size, 0);
}

TEST(BSTACK_TLV, TestWriteRead){
	uint32_t tag = TAG('a','b','c','d');
	uint8_t  data = 7;
	uint8_t  buffer = data;
	btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, &buffer, 1);
	int size = btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, NULL, 0);
	CHECK_EQUAL(size, 1);
	btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, &buffer, 1);
	CHECK_EQUAL(buffer, data);
}

TEST(BSTACK_TLV, TestWriteWriteRead){
	uint32_t tag = TAG('a','b','c','d');
	uint8_t  data = 7;
	uint8_t  buffer = data;
	btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, &buffer, 1);
	data++;
	buffer = data;
	btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, &buffer, 1);
	int size = btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, NULL, 0);
	CHECK_EQUAL(size, 1);
	btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, &buffer, 1);
	CHECK_EQUAL(buffer, data);
}

TEST(BSTACK_TLV, TestWriteABARead){
	uint32_t tag_a = TAG('a','a','a','a');
	uint32_t tag_b = TAG('b','b','b','b');;
	uint8_t  data = 7;
	uint8_t  buffer = data;
	btstack_tlv_impl->store_tag(&btstack_tlv_context, tag_a, &buffer, 1);
 	data++;
	buffer = data;
	btstack_tlv_impl->store_tag(&btstack_tlv_context, tag_b, &buffer, 1);
	data++;
	buffer = data;
	btstack_tlv_impl->store_tag(&btstack_tlv_context, tag_a, &buffer, 1);
	int size = btstack_tlv_impl->get_tag(&btstack_tlv_context, tag_a, NULL, 0);
	CHECK_EQUAL(size, 1);
	btstack_tlv_impl->get_tag(&btstack_tlv_context, tag_a, &buffer, 1);
	CHECK_EQUAL(buffer, data);
}

TEST(BSTACK_TLV, TestWriteDeleteRead){
	uint32_t tag = TAG('a','b','c','d');
	uint8_t  data = 7;
	uint8_t  buffer = data;
	btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, &buffer, 1);
	data++;
	buffer = data;
	btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, &buffer, 1);
	btstack_tlv_impl->delete_tag(&btstack_tlv_context, tag);
	int size = btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, NULL, 0);
	CHECK_EQUAL(size, 0);
}

TEST(BSTACK_TLV, TestMigrate){
	uint32_t tag = TAG('a','b','c','d');
	uint8_t  data[8];
	memcpy(data, "01234567", 8);

	// entry 8 + data 8 = 16. 
	int i;
	for (i=0;i<8;i++){
		data[0] = '0' + i;
		btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, &data[0], 8);
	}

	reopen_db();

	uint8_t buffer[8];
	btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, &buffer[0], 1);
	CHECK_EQUAL(buffer[0], data[0]);
}

TEST(BSTACK_TLV, TestMigrate2){
	uint32_t tag1 = 0x11223344;
	uint32_t tag2 = 0x44556677;
	uint8_t  data1[8];
	memcpy(data1, "01234567", 8);
	uint8_t  data2[8];
	memcpy(data2, "abcdefgh", 8);

	// entry 8 + data 8 = 16. 
	int i;
	for (i=0;i<8;i++){
		data1[0] = '0' + i;
		data2[0] = 'a' + i;
		btstack_tlv_impl->store_tag(&btstack_tlv_context, tag1, data1, 8);
		btstack_tlv_impl->store_tag(&btstack_tlv_context, tag2, data2, 8);
	}

	reopen_db();

	uint8_t buffer[8];
	btstack_tlv_impl->get_tag(&btstack_tlv_context, tag1, &buffer[0], 1);
	CHECK_EQUAL(buffer[0], data1[0]);
	btstack_tlv_impl->get_tag(&btstack_tlv_context, tag2, &buffer[0], 1);
	CHECK_EQUAL(buffer[0], data2[0]);
}

TEST(BSTACK_TLV, TestWriteResetRead){
	uint32_t tag = TAG('a','b','c','d');
    uint8_t  data = 7;
    uint8_t  buffer = data;
    btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, &buffer, 1);

    reopen_db();

    int size = btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, NULL, 0);
    CHECK_EQUAL(size, 1);
    btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, &buffer, 1);
    CHECK_EQUAL(buffer, data);
}

TEST(BSTACK_TLV, TestWriteDeleteResetReadDeleteRead){
    uint32_t tag = TAG('a','b','c','d');
    uint8_t  data = 7;
    uint8_t  buffer = data;
    int size;

    btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, &buffer, 1);
    btstack_tlv_impl->delete_tag(&btstack_tlv_context, tag);

    reopen_db();

    size = btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, NULL, 0);
    CHECK_EQUAL(size, 0);

    // assert that it's really gone and not just shadowed by delete tag
    btstack_tlv_impl->delete_tag(&btstack_tlv_context, tag);

    size = btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, NULL, 0);
    CHECK_EQUAL(size, 0);
}


int main (int argc, const char * argv[]){
    // log into file using HCI_DUMP_PACKETLOGGER format
    const char * log_path = "hci_dump.pklg";
    hci_dump_posix_fs_open(log_path, HCI_DUMP_PACKETLOGGER);
    hci_dump_init(hci_dump_posix_fs_get_instance());
    printf("Packet Log: %s\n", log_path);

    return CommandLineTestRunner::RunAllTests(argc, argv);
}
