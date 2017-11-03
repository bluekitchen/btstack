#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define L2CAP_FIXED_CHANNEL_TABLE_SIZE 3
#define FIXED_CHANNEL_FIFO_INVALID_INDEX 0xff

typedef struct l2cap_fixed_channel {
    uint8_t waiting_for_can_send_now;
    uint8_t next_request;
} l2cap_fixed_channel_t;

static l2cap_fixed_channel_t fixed_channels[L2CAP_FIXED_CHANNEL_TABLE_SIZE];
static uint8_t fixed_channel_head_index = FIXED_CHANNEL_FIFO_INVALID_INDEX;
static uint8_t fixed_channel_tail_index = FIXED_CHANNEL_FIFO_INVALID_INDEX;

static void fifo_init(void){
	printf("\ninit\n");
    memset(fixed_channels, 0, sizeof(fixed_channels));
    int i;
    for (i=0;i<L2CAP_FIXED_CHANNEL_TABLE_SIZE;i++){
        fixed_channels[i].next_request = FIXED_CHANNEL_FIFO_INVALID_INDEX;        
    }
}

static void fifo_add(int index){
	printf("add index %u\n", index);
    // insert into queue
    if (fixed_channel_tail_index == FIXED_CHANNEL_FIFO_INVALID_INDEX){
        fixed_channel_head_index = index;
    } else {
        fixed_channels[fixed_channel_tail_index].next_request = index;
    }
    fixed_channel_tail_index = index;
    fixed_channels[index].next_request = FIXED_CHANNEL_FIFO_INVALID_INDEX;
    fixed_channels[index].waiting_for_can_send_now = 1;
}

static int fifo_get_next(void){
    if (fixed_channel_head_index == FIXED_CHANNEL_FIFO_INVALID_INDEX) return -1;

    int can_send = 0;
    uint8_t i = fixed_channel_head_index;
    if (fixed_channels[i].waiting_for_can_send_now){
    	can_send = 1;
    }
    fixed_channels[i].waiting_for_can_send_now = 0;
    fixed_channel_head_index = fixed_channels[i].next_request;
    fixed_channels[i].next_request = FIXED_CHANNEL_FIFO_INVALID_INDEX;
    if (fixed_channel_head_index == FIXED_CHANNEL_FIFO_INVALID_INDEX){
        fixed_channel_tail_index = FIXED_CHANNEL_FIFO_INVALID_INDEX;
    }
    if (can_send) {
    	return i;
    }
    return -1;
}

static void fifo_dump(void){
	while (1){
		int i = fifo_get_next();
		if (i < 0) break;
		printf("got index %u\n", i);
	}
}

static void test1(void){
	fifo_init();
	fifo_add(1);
	fifo_dump();
}

static void test2(void){
	fifo_init();
	fifo_add(1);
	fifo_add(2);
	fifo_dump();
}

static void test3(void){
	fifo_init();
	fifo_add(1);
	int index;
	index = fifo_get_next();
	printf("got %u\n", index);
	fifo_add(2);
	fifo_add(1);
	fifo_add(0);
	fifo_dump();
}

int main(int argc, const char ** argv){
	(void) argc;
	(void) argv;
	test1();
	test2();
	test3();
}
