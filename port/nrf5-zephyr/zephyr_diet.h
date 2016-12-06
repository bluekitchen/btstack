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

inline void * btstack_buf_add(btstack_buf_t * buf, uint16_t len){
	void * data = buf->data;
	buf->data += len;	// probably wrong, but doesn't matter currently
	buf->len  += len;
	return data;
}

inline void * btstack_buf_pull(btstack_buf_t * buf, uint16_t len){
	buf->data += len;
	buf->len  -= len;
	return buf->data;
}

#endif
