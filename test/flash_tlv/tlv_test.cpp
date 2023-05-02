
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "hal_flash_bank.h"
#include "hal_flash_bank_memory.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "hci_dump.h"
#include "hci_dump_posix_fs.h"
#include "classic/btstack_link_key_db.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "btstack_util.h"
#include "btstack_config.h"
#include "btstack_debug.h"

#ifdef ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD
// Provide additional bytes for 3 x delete fields (in both banks)
#define HAL_FLASH_BANK_MEMORY_STORAGE_SIZE (256 + 24)
#define TAG_OVERHEAD 16
#else
#define HAL_FLASH_BANK_MEMORY_STORAGE_SIZE (256)
#define TAG_OVERHEAD 8
#endif
#define HAL_FLASH_BANK_MEMORY_BANK_SIZE (HAL_FLASH_BANK_MEMORY_STORAGE_SIZE / 2)



static uint8_t hal_flash_bank_memory_storage[HAL_FLASH_BANK_MEMORY_STORAGE_SIZE];

static void CHECK_EQUAL_ARRAY(uint8_t * expected, uint8_t * actual, int size){
	int i;
	for (i=0; i<size; i++){
		if (expected[i] != actual[i]) {
			printf("offset %u wrong\n", i);
			printf("expected: "); printf_hexdump(expected, size);
			printf("actual:   "); printf_hexdump(actual, size);
		}
		BYTES_EQUAL(expected[i], actual[i]);
	}
}

TEST_GROUP(HAL_FLASH_bank){
	const hal_flash_bank_t * hal_flash_bank_impl;
	hal_flash_bank_memory_t hal_flash_bank_context;
    void setup(void){
    	hal_flash_bank_impl = hal_flash_bank_memory_init_instance(&hal_flash_bank_context, hal_flash_bank_memory_storage, HAL_FLASH_BANK_MEMORY_STORAGE_SIZE);
		hal_flash_bank_impl->erase(&hal_flash_bank_context, 0);
		hal_flash_bank_impl->erase(&hal_flash_bank_context, 1);
    }
};

TEST(HAL_FLASH_bank, TestErased){
	uint8_t buffer;
	int offsets[] = { 0, 10, 100};
	int i;
	for (i=0;i<sizeof(offsets)/sizeof(int);i++){
		int bank;
		for (bank=0;bank<2;bank++){
			hal_flash_bank_impl->read(&hal_flash_bank_context, bank, offsets[i], &buffer, 1);	
		    CHECK_EQUAL(buffer, 0xff);
		}
	}
}

TEST(HAL_FLASH_bank, TestWrite){
	uint8_t buffer;
	int offsets[] = { 0, 10, 100};
	int i;
	for (i=0;i<sizeof(offsets)/sizeof(int);i++){
		int bank;
		for (bank=0;bank<2;bank++){
			buffer = i;
			hal_flash_bank_impl->write(&hal_flash_bank_context, bank, offsets[i], &buffer, 1);	
		}
	}
	for (i=0;i<sizeof(offsets)/sizeof(int);i++){
		int bank;
		for (bank=0;bank<2;bank++){
			hal_flash_bank_impl->read(&hal_flash_bank_context, bank, offsets[i], &buffer, 1);	
		    CHECK_EQUAL(buffer, i);
		}
	}
}

#if 0
// prints error and exits tests. maybe all functions need to return ok
TEST(HAL_FLASH_bank, TestWriteTwice){
	uint8_t buffer = 5;
	hal_flash_bank_impl->write(&hal_flash_bank_context, 0, 5, &buffer, 1);	
	hal_flash_bank_impl->write(&hal_flash_bank_context, 0, 5, &buffer, 1);	
}
#endif

TEST(HAL_FLASH_bank, TestWriteErase){
	uint32_t offset = 7;
	uint8_t value = 9;
	uint8_t buffer = value;
	hal_flash_bank_impl->write(&hal_flash_bank_context, 0, offset, &buffer, 1);
	hal_flash_bank_impl->read(&hal_flash_bank_context, 0, offset, &buffer, 1);	
    CHECK_EQUAL(buffer, value);
	hal_flash_bank_impl->erase(&hal_flash_bank_context, 0);
	hal_flash_bank_impl->read(&hal_flash_bank_context, 0, offset, &buffer, 1);	
    CHECK_EQUAL(buffer, 0xff);
}

/// TLV
TEST_GROUP(BSTACK_TLV){
	
	const hal_flash_bank_t * hal_flash_bank_impl;
	hal_flash_bank_memory_t  hal_flash_bank_context;

	const btstack_tlv_t *    btstack_tlv_impl;
	btstack_tlv_flash_bank_t btstack_tlv_context;

    void setup(void){
    	hal_flash_bank_impl = hal_flash_bank_memory_init_instance(&hal_flash_bank_context, hal_flash_bank_memory_storage, HAL_FLASH_BANK_MEMORY_STORAGE_SIZE);
		hal_flash_bank_impl->erase(&hal_flash_bank_context, 0);
		hal_flash_bank_impl->erase(&hal_flash_bank_context, 1);
    }
};

TEST(BSTACK_TLV, TestMissingTag){
	btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);
	uint32_t tag = 'abcd';
	int size = btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, NULL, 0);
	CHECK_EQUAL(size, 0);
}

TEST(BSTACK_TLV, TestWriteRead){
	btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);
	uint32_t tag = 'abcd';
	uint8_t  data = 7;
	uint8_t  buffer = data;
	btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, &buffer, 1);
	int size = btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, NULL, 0);
	CHECK_EQUAL(size, 1);
	btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, &buffer, 1);
	CHECK_EQUAL(buffer, data);
}

TEST(BSTACK_TLV, TestWriteWriteRead){
	btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);
	uint32_t tag = 'abcd';
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
	btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);
	uint32_t tag_a = 'aaaa';
	uint32_t tag_b = 'bbbb';
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
	btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);
	uint32_t tag = 'abcd';
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

	btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);

	uint32_t tag = 'abcd';
	uint8_t  data[8];
	memcpy(data, "01234567", 8);

	// entry 8 + data 8 = 16. 
	int i;
	for (i=0;i<8;i++){
		data[0] = '0' + i;
		btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, &data[0], 8);
	}

	btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);

	uint8_t buffer[8];
	btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, &buffer[0], 1);
	CHECK_EQUAL(buffer[0], data[0]);
}

TEST(BSTACK_TLV, TestMigrate2){

	btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);

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

	btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);

	uint8_t buffer[8];
	btstack_tlv_impl->get_tag(&btstack_tlv_context, tag1, &buffer[0], 1);
	CHECK_EQUAL(buffer[0], data1[0]);
	btstack_tlv_impl->get_tag(&btstack_tlv_context, tag2, &buffer[0], 1);
	CHECK_EQUAL(buffer[0], data2[0]);
}

TEST(BSTACK_TLV, TestWriteResetRead){
    btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);
    uint32_t tag = 'abcd';
    uint8_t  data = 7;
    uint8_t  buffer = data;
    btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, &buffer, 1);
    btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);
    int size = btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, NULL, 0);
    CHECK_EQUAL(size, 1);
    btstack_tlv_impl->get_tag(&btstack_tlv_context, tag, &buffer, 1);
    CHECK_EQUAL(buffer, data);
}

TEST(BSTACK_TLV, TestFullBank){
    btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);

    // fill-up flash bank
    uint32_t tag = 'abcd';
    uint8_t blob[HAL_FLASH_BANK_MEMORY_BANK_SIZE - 8 - TAG_OVERHEAD];
    btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, (uint8_t *) &blob, sizeof(blob));
    CHECK_EQUAL(0, btstack_tlv_context.current_bank);
    CHECK_EQUAL(HAL_FLASH_BANK_MEMORY_BANK_SIZE, btstack_tlv_context.write_offset);

    // check
    btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);
    CHECK_EQUAL(0, btstack_tlv_context.current_bank);
    CHECK_EQUAL(HAL_FLASH_BANK_MEMORY_BANK_SIZE, btstack_tlv_context.write_offset);
}

TEST(BSTACK_TLV, TestAlmostFullBank){
    btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);

    // fill-up flash bank without the last byte
    uint32_t tag = 'abcd';
    uint8_t blob[HAL_FLASH_BANK_MEMORY_BANK_SIZE - 8 - TAG_OVERHEAD - 1];
    btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, (uint8_t *) &blob, sizeof(blob));
    CHECK_EQUAL(0, btstack_tlv_context.current_bank);
    CHECK_EQUAL(HAL_FLASH_BANK_MEMORY_BANK_SIZE-1, btstack_tlv_context.write_offset);

    // check
    btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);
    CHECK_EQUAL(0, btstack_tlv_context.current_bank);
    CHECK_EQUAL(HAL_FLASH_BANK_MEMORY_BANK_SIZE-1, btstack_tlv_context.write_offset);
}

TEST(BSTACK_TLV, TestFullBankPlusMigrate){
    btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);

    // fill-up flash bank
    uint32_t tag = 'abcd';
    uint8_t blob[((HAL_FLASH_BANK_MEMORY_BANK_SIZE - 8) / 2) - TAG_OVERHEAD];
    btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, (uint8_t *) &blob, sizeof(blob));
    btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, (uint8_t *) &blob, sizeof(blob));
    CHECK_EQUAL(0, btstack_tlv_context.current_bank);
    CHECK_EQUAL(HAL_FLASH_BANK_MEMORY_BANK_SIZE, btstack_tlv_context.write_offset);

    // check
    btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);
    CHECK_EQUAL(0, btstack_tlv_context.current_bank);
    CHECK_EQUAL(HAL_FLASH_BANK_MEMORY_BANK_SIZE, btstack_tlv_context.write_offset);

    // store one more -> trigger migration
    btstack_tlv_impl->store_tag(&btstack_tlv_context, tag, (uint8_t *) &blob, sizeof(blob));
    CHECK_EQUAL(1, btstack_tlv_context.current_bank);
    CHECK_EQUAL(8 + 2 * (TAG_OVERHEAD + sizeof(blob)), btstack_tlv_context.write_offset);
}

//
TEST_GROUP(LINK_KEY_DB){
	const hal_flash_bank_t * hal_flash_bank_impl;
	hal_flash_bank_memory_t  hal_flash_bank_context;

	const btstack_tlv_t *      btstack_tlv_impl;
	btstack_tlv_flash_bank_t btstack_tlv_context;

	const btstack_link_key_db_t * btstack_link_key_db;

    bd_addr_t addr1, addr2, addr3;
    link_key_t link_key1, link_key2;
    link_key_type_t link_key_type;

    void setup(void){
    	// hal_flash_bank
    	hal_flash_bank_impl = hal_flash_bank_memory_init_instance(&hal_flash_bank_context, hal_flash_bank_memory_storage, HAL_FLASH_BANK_MEMORY_STORAGE_SIZE);
		hal_flash_bank_impl->erase(&hal_flash_bank_context, 0);
		hal_flash_bank_impl->erase(&hal_flash_bank_context, 1);
		// btstack_tlv
		btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(&btstack_tlv_context, hal_flash_bank_impl, &hal_flash_bank_context);
		// btstack_link_key_db
		btstack_link_key_db = btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, &btstack_tlv_context);

        bd_addr_t addr_1 = {0x00, 0x01, 0x02, 0x03, 0x04, 0x01 };
        bd_addr_t addr_2 = {0x00, 0x01, 0x02, 0x03, 0x04, 0x02 };
        bd_addr_t addr_3 = {0x00, 0x01, 0x02, 0x03, 0x04, 0x03 };
        bd_addr_copy(addr1, addr_1); 
        bd_addr_copy(addr2, addr_2); 
        bd_addr_copy(addr3, addr_3); 
        for (int i=0;i<16;i++) {
        	link_key1[i] = 'a'+i;
        	link_key2[i] = 'A'+i;
        }
        link_key_type = COMBINATION_KEY;
    }
};

TEST(LINK_KEY_DB, SinglePutGetDeleteKey){

	link_key_t test_link_key;
    link_key_type_t test_link_key_type;

    btstack_link_key_db->delete_link_key(addr1);
    CHECK(btstack_link_key_db->get_link_key(addr1, test_link_key, &test_link_key_type) == 0);
    
	btstack_link_key_db->put_link_key(addr1, link_key1, link_key_type);
    CHECK(btstack_link_key_db->get_link_key(addr1, test_link_key, &test_link_key_type) == 1);
    CHECK_EQUAL_ARRAY(link_key1, test_link_key, 16);
    
    btstack_link_key_db->delete_link_key(addr1);
    CHECK(btstack_link_key_db->get_link_key(addr1, test_link_key, &test_link_key_type) == 0);
}

TEST(LINK_KEY_DB, UpdateKey){
	link_key_t test_link_key;
    link_key_type_t test_link_key_type;

	btstack_link_key_db->put_link_key(addr1, link_key1, link_key_type);
	btstack_link_key_db->put_link_key(addr1, link_key2, link_key_type);
    CHECK(btstack_link_key_db->get_link_key(addr1, test_link_key, &test_link_key_type) == 1);
    CHECK_EQUAL_ARRAY(link_key2, test_link_key, 16);
}

TEST(LINK_KEY_DB, NumKeys){
    CHECK(NVM_NUM_LINK_KEYS ==  2);
}

TEST(LINK_KEY_DB, KeyReplacement){
	link_key_t test_link_key;
    link_key_type_t test_link_key_type;

	btstack_link_key_db->put_link_key(addr1, link_key1, link_key_type);
	btstack_link_key_db->put_link_key(addr2, link_key1, link_key_type);
	btstack_link_key_db->put_link_key(addr3, link_key1, link_key_type);

    CHECK(btstack_link_key_db->get_link_key(addr3, test_link_key, &test_link_key_type) == 1);
    CHECK(btstack_link_key_db->get_link_key(addr2, test_link_key, &test_link_key_type) == 1);
    CHECK(btstack_link_key_db->get_link_key(addr1, test_link_key, &test_link_key_type) == 0);
    CHECK_EQUAL_ARRAY(link_key1, test_link_key, 16);
}

int main (int argc, const char * argv[]){
    // log into file using HCI_DUMP_PACKETLOGGER format
#ifdef ENABLE_TLV_FLASH_WRITE_ONCE
    const char * pklg_path = "hci_dump_write_once.pklg";
#else
    const char * pklg_path = "hci_dump.pklg";
#endif
    hci_dump_posix_fs_open(pklg_path, HCI_DUMP_PACKETLOGGER);
    hci_dump_init(hci_dump_posix_fs_get_instance());
    printf("Packet Log: %s\n", pklg_path);

    return CommandLineTestRunner::RunAllTests(argc, argv);
}
