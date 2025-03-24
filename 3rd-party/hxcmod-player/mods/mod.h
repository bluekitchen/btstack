#ifndef _MOD_H_
#define _MOD_H_

#include <stdint.h>
#include <stddef.h>

typedef struct {
    size_t len;
    char *name;
    const uint8_t *data;
} mod_title_t;

extern const mod_title_t titles[];
extern const mod_title_t *default_mod_title;

#endif // _MOD_H_

