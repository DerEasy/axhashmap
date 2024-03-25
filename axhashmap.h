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

axhashmap *axh_setToHash(axhashmap *h, uint64_t (*toHash)(const void *, uint64_t (*)(const void *, size_t)));

uint64_t (*axh_getToHash(axhashmap *h))(const void *, uint64_t (*)(const void *, size_t));

axhashmap *axh_setDestructor(axhashmap *h, void (*destroy)(void *, void *));

void (*axh_getDestructor(axhashmap *h))(void *, void *);


axhashmap *axh_sizedNew(uint64_t size, uint64_t span, double loadFactor);

axhashmap *axh_new(uint64_t span);

void axh_destroy(axhashmap *h);

bool axh_rehash(axhashmap *h, uint64_t tableSize);

int axh_map(axhashmap *h, void *key, void *value);

int axh_add(axhashmap *h, void *key);

bool axh_has(axhashmap *h, void *key);

void *axh_get(axhashmap *h, void *key);

bool axh_remove(axhashmap *h, void *key);

#endif //AXHASH_AXHASHMAP_H
