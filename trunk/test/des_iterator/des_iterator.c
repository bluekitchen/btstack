#include "des_iterator.h"
 
int des_iterator_init(des_iterator_t * it, uint8_t * element){
	de_type_t type = de_get_element_type(element);
    if (type != DE_DES) return 0;

	it->element = element;
    it->pos = de_get_header_size(element);
    it->length = de_get_len(element);
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

int  des_iterator_has_more(des_iterator_t * it){
	return 0;
}

uint8_t * des_iterator_get_value(des_iterator_t * it){
	return NULL;
}
