
#ifndef __DES_ITERATOR_H
#define __DES_ITERATOR_H

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/sdp_util.h>
#include <btstack/utils.h>

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
	uint8_t * element;
	uint16_t pos;
	uint16_t length;
} des_iterator_t;

int des_iterator_init(des_iterator_t * it, uint8_t * element);
int  des_iterator_has_more(des_iterator_t * it);
de_type_t des_iterator_get_type (des_iterator_t * it);
uint16_t des_iterator_get_size (des_iterator_t * it);
uint8_t * des_iterator_get_element(des_iterator_t * it);
void des_iterator_next(des_iterator_t * it);

#if defined __cplusplus
}
#endif

#endif // __DES_ITERATOR_H