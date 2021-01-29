#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "btstack_linked_list.h"

btstack_linked_list_t testList;
btstack_linked_item_t itemA;
btstack_linked_item_t itemB;
btstack_linked_item_t itemC;
btstack_linked_item_t itemD;

TEST_GROUP(LinkedListEmpty){
    void setup(void){
        testList = NULL;
    }
};

TEST(LinkedListEmpty, CountAll){
    CHECK_EQUAL(0, btstack_linked_list_count(&testList)); 
}

TEST(LinkedListEmpty, IsEmpty){
    CHECK_EQUAL(true, btstack_linked_list_empty(&testList));
}

TEST(LinkedListEmpty, Addtail){
    CHECK_EQUAL(true, btstack_linked_list_add_tail(&testList, &itemA));
}

TEST(LinkedListEmpty, RemoveNonExisting){
    CHECK_EQUAL(false, btstack_linked_list_remove(&testList, &itemA));
}

TEST_GROUP(LinkedList){
    void setup(void){
        testList = NULL;
        btstack_linked_list_add(&testList, &itemD);
        btstack_linked_list_add(&testList, &itemC);
        btstack_linked_list_add(&testList, &itemB);
        btstack_linked_list_add(&testList, &itemA);
    }
};

TEST(LinkedList, CountAll){
    CHECK_EQUAL(4, btstack_linked_list_count(&testList)); 
}

TEST(LinkedList, GetFirst){
    btstack_linked_item_t * item;
    item = btstack_linked_list_get_first_item(&testList);
    CHECK_EQUAL(item, &itemA);
}

TEST(LinkedList, GetLast){
    btstack_linked_item_t * item;
    item = btstack_linked_list_get_last_item(&testList);
    CHECK_EQUAL(item, &itemD);
}

TEST(LinkedList, Pop){
    btstack_linked_item_t * item;
    item = btstack_linked_list_pop(&testList);
    CHECK_EQUAL(item, &itemA);
    CHECK_EQUAL(3, btstack_linked_list_count(&testList)); 
}

TEST(LinkedList, AddExisting){
    CHECK_EQUAL( false, btstack_linked_list_add(&testList, &itemD));
}

TEST(LinkedList, AddTailExisting){
CHECK_EQUAL( false, btstack_linked_list_add_tail(&testList, &itemD));
}

TEST(LinkedList, Iterator){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &testList);
    btstack_linked_item_t * item;
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemA);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!btstack_linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveFirstUsingIterator){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &testList);
    btstack_linked_item_t * item;
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemA);
    btstack_linked_list_iterator_remove(&it);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!btstack_linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveFirstUsingListRemove){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &testList);
    btstack_linked_item_t * item;
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemA);
    btstack_linked_list_remove(&testList, &itemA);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!btstack_linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveSecondUsingIterator){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &testList);
    btstack_linked_item_t * item;
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    btstack_linked_list_iterator_remove(&it);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!btstack_linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveSecondUsingListRemove){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &testList);
    btstack_linked_item_t * item;
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    btstack_linked_list_remove(&testList, &itemB);
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!btstack_linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveThirdUsingIterator){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &testList);
    btstack_linked_item_t * item;
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    btstack_linked_list_iterator_remove(&it);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!btstack_linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveLastUsingIterator){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &testList);
    btstack_linked_item_t * item;
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    btstack_linked_list_iterator_remove(&it);
    CHECK(!btstack_linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveLastUsingListRemove){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &testList);
    btstack_linked_item_t * item;
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    btstack_linked_list_remove(&testList, &itemD);
    CHECK(!btstack_linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveSecondAndThirdUsingIterator){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &testList);
    btstack_linked_item_t * item;
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    btstack_linked_list_iterator_remove(&it);   // B
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemC);
    btstack_linked_list_iterator_remove(&it);   // C
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!btstack_linked_list_iterator_has_next(&it));
}

TEST(LinkedList, RemoveSecondAndThirdUsingListRemove){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &testList);
    btstack_linked_item_t * item;
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemB);
    btstack_linked_list_remove(&testList, &itemB);
    btstack_linked_list_remove(&testList, &itemC);
    CHECK(btstack_linked_list_iterator_has_next(&it));
    item = btstack_linked_list_iterator_next(&it);
    CHECK_EQUAL(item, &itemD);
    CHECK(!btstack_linked_list_iterator_has_next(&it));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}