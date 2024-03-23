//
// Created by easy on 20.03.24.
//

#ifndef AXHASH_AXHASHMAP_H
#define AXHASH_AXHASHMAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct axhashmap axhashmap;


uint64_t axh_size(axhashmap *h);

uint64_t axh_tableSize(axhashmap *h);

axhashmap *axh_setLoadFactor(axhashmap *h, double lf);

double axh_getLoadFactor(axhashmap *h);

axhashmap *axh_setDynamicSpan(axhashmap *h, size_t (*dynamicSpan)(const void *));

size_t (*axh_getDynamicSpan(axhashmap *h))(const void *);

axhashmap *axh_setDestructor(axhashmap *h, void (*destroy)(void *, void *));

void (*axh_getDestructor(axhashmap *h))(void *, void *);


axhashmap *axh_sizedNew(size_t size, size_t span, double loadFactor);

axhashmap *axh_new(size_t span);

void axh_destroy(axhashmap *h);

bool axh_rehash(axhashmap *h, size_t size);

int axh_sizedMap(axhashmap *h, void *key, void *value, size_t size);

int axh_map(axhashmap *h, void *key, void *value);

int axh_add(axhashmap *h, void *key);

bool axh_has(axhashmap *h, void *key);

void *axh_get(axhashmap *h, void *key);

bool axh_remove(axhashmap *h, void *key);

#endif //AXHASH_AXHASHMAP_H
