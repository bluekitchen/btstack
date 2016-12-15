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

// from 

/**
 * Configure a static interrupt.
 *
 * All arguments must be computable by the compiler at build time.
 *
 * Internally this function does a few things:
 *
 * 1. The enum statement has no effect but forces the compiler to only
 * accept constant values for the irq_p parameter, very important as the
 * numerical IRQ line is used to create a named section.
 *
 * 2. An instance of _IsrTableEntry is created containing the ISR and its
 * parameter. If you look at how _sw_isr_table is created, each entry in the
 * array is in its own section named by the IRQ line number. What we are doing
 * here is to override one of the default entries (which points to the
 * spurious IRQ handler) with what was supplied here.
 *
 * 3. The priority level for the interrupt is configured by a call to
 * _irq_priority_set()
 *
 * @param irq_p IRQ line number
 * @param priority_p Interrupt priority
 * @param isr_p Interrupt service routine
 * @param isr_param_p ISR parameter
 * @param flags_p IRQ options
 *
 * @return The vector assigned to this interrupt
 */
#define MY_IRQ_CONNECT(irq_p, priority_p, isr_p, isr_param_p, flags_p) \
({ \
	enum { IRQ = irq_p }; \
	static struct _IsrTableEntry _CONCAT(_isr_irq, irq_p) \
		__attribute__ ((used)) \
		__attribute__ ((section(STRINGIFY(_CONCAT(.gnu.linkonce.isr_irq, irq_p))))) = \
			{isr_param_p, isr_p}; \
	_irq_priority_set(irq_p, priority_p, flags_p); \
	irq_p; \
})

#endif
