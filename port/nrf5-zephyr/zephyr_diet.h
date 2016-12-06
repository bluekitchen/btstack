#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

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
	buf->data += len;
	buf->len  += len;
	return data;
}

#endif
