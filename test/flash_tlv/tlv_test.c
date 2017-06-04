
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "hal_flash_sector.h"
#include "hal_flash_sector_memory.h"
#include "btstack_tlv.h"
#include "hci_dump.h"

TEST_GROUP(HAL_FLASH_SECTOR){
	const hal_flash_sector_t * hal_flash_sector;
    void setup(void){
    	hal_flash_sector = hal_flash_sector_memory_get_instance();
		hal_flash_sector->erase(0);
		hal_flash_sector->erase(1);
    }
};

TEST(HAL_FLASH_SECTOR, TestErased){
	uint8_t buffer;
	int offsets[] = { 0, 10, 100};
	int i;
	for (i=0;i<sizeof(offsets)/sizeof(int);i++){
		int bank;
		for (bank=0;bank<2;bank++){
			hal_flash_sector->read(bank, offsets[i], &buffer, 1);	
		    CHECK_EQUAL(buffer, 0xff);
		}
	}
}

TEST(HAL_FLASH_SECTOR, TestWrite){
	uint8_t buffer;
	int offsets[] = { 0, 10, 100};
	int i;
	for (i=0;i<sizeof(offsets)/sizeof(int);i++){
		int bank;
		for (bank=0;bank<2;bank++){
			buffer = i;
			hal_flash_sector->write(bank, offsets[i], &buffer, 1);	
		}
	}
	for (i=0;i<sizeof(offsets)/sizeof(int);i++){
		int bank;
		for (bank=0;bank<2;bank++){
			hal_flash_sector->read(bank, offsets[i], &buffer, 1);	
		    CHECK_EQUAL(buffer, i);
		}
	}
}

#if 0
// prints error and exits tests. maybe all functions need to return ok
TEST(HAL_FLASH_SECTOR, TestWriteTwice){
	uint8_t buffer = 5;
	hal_flash_sector->write(0, 5, &buffer, 1);	
	hal_flash_sector->write(0, 5, &buffer, 1);	
}
#endif

TEST(HAL_FLASH_SECTOR, TestWriteErase){
	uint32_t offset = 7;
	uint8_t value = 9;
	uint8_t buffer = value;
	hal_flash_sector->write(0, offset, &buffer, 1);
	hal_flash_sector->read(0, offset, &buffer, 1);	
    CHECK_EQUAL(buffer, value);
	hal_flash_sector->erase(0);
	hal_flash_sector->read(0, offset, &buffer, 1);	
    CHECK_EQUAL(buffer, 0xff);
}

TEST_GROUP(BSTACK_TLV){
	const hal_flash_sector_t * hal_flash_sector;
    void setup(void){
    	hal_flash_sector = hal_flash_sector_memory_get_instance();
		hal_flash_sector->erase(0);
		hal_flash_sector->erase(1);
    }
};

TEST(BSTACK_TLV, TestMissingTag){
	btstack_tlv_init(hal_flash_sector);
	uint32_t tag = 'abcd';
	int size = btstack_tlv_get_tag(tag, NULL, 0);
	CHECK_EQUAL(size, 0);
}

TEST(BSTACK_TLV, TestWriteRead){
	btstack_tlv_init(hal_flash_sector);
	uint32_t tag = 'abcd';
	uint8_t  data = 7;
	uint8_t  buffer = data;
	btstack_tlv_store_tag(tag, &buffer, 1);
	int size = btstack_tlv_get_tag(tag, NULL, 0);
	CHECK_EQUAL(size, 1);
	btstack_tlv_get_tag(tag, &buffer, 1);
	CHECK_EQUAL(buffer, data);
}

TEST(BSTACK_TLV, TestWriteWriteRead){
	btstack_tlv_init(hal_flash_sector);
	uint32_t tag = 'abcd';
	uint8_t  data = 7;
	uint8_t  buffer = data;
	btstack_tlv_store_tag(tag, &buffer, 1);
	data++;
	buffer = data;
	btstack_tlv_store_tag(tag, &buffer, 1);
	int size = btstack_tlv_get_tag(tag, NULL, 0);
	CHECK_EQUAL(size, 1);
	btstack_tlv_get_tag(tag, &buffer, 1);
	CHECK_EQUAL(buffer, data);
}

TEST(BSTACK_TLV, TestWriteDeleteRead){
	btstack_tlv_init(hal_flash_sector);
	uint32_t tag = 'abcd';
	uint8_t  data = 7;
	uint8_t  buffer = data;
	btstack_tlv_store_tag(tag, &buffer, 1);
	data++;
	buffer = data;
	btstack_tlv_store_tag(tag, &buffer, 1);
	btstack_tlv_delete_tag(tag);
	int size = btstack_tlv_get_tag(tag, NULL, 0);
	CHECK_EQUAL(size, 0);
}

TEST(BSTACK_TLV, TestMigrate){

	btstack_tlv_init(hal_flash_sector);

	uint32_t tag = 'abcd';
	uint8_t  data[8];
	memcpy(data, "01234567", 8);

	// entry 8 + data 8 = 16. 
	int i;
	for (i=0;i<8;i++){
		data[0] = '0' + i;
		btstack_tlv_store_tag(tag, &data[0], 8);
	}

	btstack_tlv_init(hal_flash_sector);

	uint8_t buffer[8];
	btstack_tlv_get_tag(tag, &buffer[0], 1);
	CHECK_EQUAL(buffer[0], data[0]);
}

TEST(BSTACK_TLV, TestMigrate2){

	btstack_tlv_init(hal_flash_sector);

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
		btstack_tlv_store_tag(tag1, data1, 8);
		btstack_tlv_store_tag(tag2, data2, 8);
	}

	btstack_tlv_init(hal_flash_sector);

	uint8_t buffer[8];
	btstack_tlv_get_tag(tag1, &buffer[0], 1);
	CHECK_EQUAL(buffer[0], data1[0]);
	btstack_tlv_get_tag(tag2, &buffer[0], 1);
	CHECK_EQUAL(buffer[0], data2[0]);
}

int main (int argc, const char * argv[]){
	hci_dump_open("tlv_test.pklg", HCI_DUMP_PACKETLOGGER);
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
