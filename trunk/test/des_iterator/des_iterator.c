#include "des_iterator.h"

int des_iterator_init(des_iterator_t * it, uint8_t * element){
	de_type_t type = de_get_element_type(element);
    if (type != DE_DES) return 0;

	it->element = element;
    it->pos = de_get_header_size(element);
    it->length = de_get_len(element);
    // printf("des_iterator_init current pos %d, total len %d\n", it->pos, it->length);
    return 1;
}

de_type_t des_iterator_get_type (des_iterator_t * it){
	return de_get_element_type(&it->element[it->pos]);
}

uint16_t des_iterator_get_size (des_iterator_t * it){
	int length = de_get_len(&it->element[it->pos]);
	int header_size = de_get_header_size(&it->element[it->pos]);
	return length - header_size;
}

int des_iterator_has_more(des_iterator_t * it){
	return it->pos < it->length;
}

uint8_t * des_iterator_get_element(des_iterator_t * it){
	if (!des_iterator_has_more(it)) return NULL;
	return &it->element[it->pos];
}

void des_iterator_next(des_iterator_t * it){
	int element_len = de_get_len(&it->element[it->pos]);
	// printf("des_iterator_next element size %d, current pos %d, total len %d\n", element_len, it->pos, it->length);
	it->pos += element_len;
}

