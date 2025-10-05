#ifndef MOD_H
#define MOD_H

#include <stdint.h>
#include <stddef.h>

/**
 * Info on mod title
 */
typedef struct {
    size_t len;
    char *name;
    const uint8_t *data;
} mod_title_t;

/**
 * Examples use title with this index as default mod song
 */
#define MOD_TITLE_DEFAULT 0

/**
 * Array of available mod titles
 */
extern const mod_title_t   mod_titles[];

/**
 * Number of titles in mod_titles
 */
extern const uint16_t      mod_titles_count;

#endif // MOD_H

