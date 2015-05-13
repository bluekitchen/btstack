#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include <btstack/linked_list.h>

linked_list_t testList;
linked_item_t itemA;
linked_item_t itemB;
linked_item_t itemC;
linked_item_t itemD;

TEST_GROUP(LinkedList){
    void setup(void){
        testList = NULL;
        linked_list_add(&testList, &itemD);
        linked_list_add(&testList, &itemC);
        linked_list_add(&testList, &itemB);
        linked_list_add(&testList, &itemA);
    }
};

TEST(LinkedList, Iterator){
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &testList);
    linked_item_t * item;
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemA);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveFirstUsingIterator){
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &testList);
    linked_item_t * item;
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemA);
    linked_list_iterator_remove(&it);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveFirstUsingListRemove){
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &testList);
    linked_item_t * item;
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemA);
    linked_list_remove(&testList, &itemA);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveSecondUsingIterator){
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &testList);
    linked_item_t * item;
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    linked_list_iterator_remove(&it);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveSecondUsingListRemove){
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &testList);
    linked_item_t * item;
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    linked_list_remove(&testList, &itemB);
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveThirdUsingIterator){
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &testList);
    linked_item_t * item;
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    linked_list_iterator_remove(&it);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveLastUsingIterator){
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &testList);
    linked_item_t * item;
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    linked_list_iterator_remove(&it);
    CHECK(!linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveLastUsingListRemove){
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &testList);
    linked_item_t * item;
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    linked_list_remove(&testList, &itemD);
    CHECK(!linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveSecondAndThirdUsingIterator){
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &testList);
    linked_item_t * item;
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    linked_list_iterator_remove(&it);   // B
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    linked_list_iterator_remove(&it);   // C
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveSecondAndThirdUsingListRemove){
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &testList);
    linked_item_t * item;
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    linked_list_remove(&testList, &itemB);
    linked_list_remove(&testList, &itemC);
    CHECK(linked_list_iterator_has_next(&it));
    item = linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!linked_list_iterator_has_next(&it));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}