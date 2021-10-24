#include "fifo.h"
#include "malloc.h"

static fifo_t g_fifo = {0};

uint8_t fifo_init(uint32_t fifo_depth)
{
	if (fifo_depth == 0) {
		return 0;
	}
	g_fifo.start = (uint8_t *)malloc(fifo_depth * sizeof(uint8_t));
	if (NULL == g_fifo.start) {
		return 0;
	}
	g_fifo.depth = fifo_depth;
	g_fifo.end = g_fifo.start + g_fifo.depth;
	g_fifo.read_ptr = g_fifo.start;
	g_fifo.write_ptr = g_fifo.start;
	g_fifo.valid_data_length = 0;
	return 1;
}

void fifo_deinit()
{
	free(g_fifo.start);
	memset(&g_fifo, 0, sizeof(fifo_t));
}

uint8_t fifo_push(uint8_t data)
{
	if (is_fifo_full()) {
		return 0;
	}
	*g_fifo.write_ptr = data;
	g_fifo.write_ptr++;
	if (g_fifo.write_ptr >= g_fifo.read_ptr) {
		g_fifo.valid_data_length = g_fifo.write_ptr - g_fifo.read_ptr;
	} else {
		g_fifo.valid_data_length = g_fifo.write_ptr - g_fifo.start + g_fifo.end - g_fifo.read_ptr;
	}
	if (g_fifo.write_ptr == g_fifo.end) {
		g_fifo.write_ptr = g_fifo.start;
	}
	return 1;
}

uint8_t fifo_pop(uint8_t *pop_data)
{
	if (is_fifo_empty()) {
		return 0;
	}
	*pop_data = *(g_fifo.read_ptr);
	*(g_fifo.read_ptr) = 0;
	g_fifo.read_ptr++;
	if (g_fifo.write_ptr >= g_fifo.read_ptr) {
		g_fifo.valid_data_length = g_fifo.write_ptr - g_fifo.read_ptr;
	}
	else {
		g_fifo.valid_data_length = g_fifo.write_ptr - g_fifo.start + g_fifo.end - g_fifo.read_ptr;
	}
	if (g_fifo.read_ptr == g_fifo.end) {
		g_fifo.read_ptr = g_fifo.start;
	}
	return 1;
}

uint8_t is_fifo_empty()
{
	return (g_fifo.valid_data_length == 0);
}

uint8_t is_fifo_full()
{
	return (g_fifo.valid_data_length == g_fifo.depth);
}

uint32_t fifo_get_valid_data_length()
{
	return g_fifo.valid_data_length;
}