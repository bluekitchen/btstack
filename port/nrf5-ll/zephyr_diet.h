#ifndef __ZEPHYR_DIET
#define __ZEPHYR_DIET

// temp global header file for zephyr_diet
#include <stdint.h>

typedef struct btstack_buf {
	/** Pointer to the start of data in the buffer. */
	uint8_t *data;

	/** Length of the data behind the data pointer. */
	uint16_t len;
} btstack_buf_t;

static inline void * btstack_buf_add(btstack_buf_t * buf, uint16_t len){
	void * data = buf->data;
	buf->data += len;	// probably wrong, but doesn't matter currently
	buf->len  += len;
	return data;
}

static inline void * btstack_buf_pull(btstack_buf_t * buf, uint16_t len){
	buf->data += len;
	buf->len  -= len;
	return buf->data;
}

//more hacks
#define LL_ASSERT(a)
#define BT_ASSERT(a)
#define sys_cpu_to_le16(a) (a)
#define sys_le16_to_cpu(a) (a)
#define BT_ERR(str) log_error(str)
static inline void sys_put_le64(uint64_t value, void * address){
	uint32_t * pointer = (uint32_t*) address;
	pointer[0] = value >> 32;
	pointer[1] = value & 0xffffffff;
}


#define BIT_MASK(a) ((1<<a)-1)
#define EINVAL 11
#endif
