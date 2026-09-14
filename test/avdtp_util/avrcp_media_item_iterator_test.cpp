/*
 * Copyright (C) 2026 BlueKitchen GmbH
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "classic/avrcp_media_item_iterator.h"

TEST_GROUP(AvrcpMediaItemIterator){};

TEST(AvrcpMediaItemIterator, reject_truncated_attribute_header){
    const uint8_t data[] = { 0, 0, 0, 0, 0, 0, 0 };
    avrcp_media_item_context_t context;
    avrcp_media_item_iterator_init(&context, sizeof(data), data);

    CHECK_FALSE(avrcp_media_item_iterator_has_more(&context));
    avrcp_media_item_iterator_next(&context);
    CHECK_FALSE(avrcp_media_item_iterator_has_more(&context));
}

TEST(AvrcpMediaItemIterator, reject_truncated_attribute_value){
    const uint8_t data[] = { 0, 0, 0, 1, 0, 0x6A, 0, 2, 0x41 };
    avrcp_media_item_context_t context;
    avrcp_media_item_iterator_init(&context, sizeof(data), data);

    CHECK_FALSE(avrcp_media_item_iterator_has_more(&context));
    avrcp_media_item_iterator_next(&context);
    CHECK_FALSE(avrcp_media_item_iterator_has_more(&context));
}

TEST(AvrcpMediaItemIterator, iterate_complete_attribute){
    const uint8_t data[] = { 0, 0, 0, 1, 0, 0x6A, 0, 1, 0x41 };
    avrcp_media_item_context_t context;
    avrcp_media_item_iterator_init(&context, sizeof(data), data);

    CHECK_TRUE(avrcp_media_item_iterator_has_more(&context));
    CHECK_EQUAL(1, avrcp_media_item_iterator_get_attr_id(&context));
    CHECK_EQUAL(1, avrcp_media_item_iterator_get_attr_value_len(&context));
    CHECK_EQUAL(0x41, avrcp_media_item_iterator_get_attr_value(&context)[0]);
    avrcp_media_item_iterator_next(&context);
    CHECK_FALSE(avrcp_media_item_iterator_has_more(&context));
}

int main(int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
