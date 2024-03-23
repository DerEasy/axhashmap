//
// Created by easy on 20.03.24.
//

#include "axhashmap.h"
#include <string.h>
#include <stdlib.h>
#include <xxhash.h>


typedef struct KeyValue {
    XXH64_hash_t hash;
    void *key;
    void *value;
} KeyValue;

struct axhashmap {
    KeyValue *table;
    double loadFactor;
    uint64_t rehashThreshold;
    uint64_t size;
    uint64_t tableSize;
    uint64_t staticSpan;
    uint64_t (*dynamicSpan)(const void *);
    void (*destroy)(void *, void *);
};


static bool isEmpty(KeyValue *kv) {
    return !kv->hash && !kv->key;
}

static uint64_t strlenSpan(const void *key) {
    return strlen(key);
}

static uint64_t mod1(uint64_t n, uint64_t k) {
    return n - (k * (n >= k));
}

static KeyValue *nextKV(axhashmap *h, KeyValue *kv) {
    ++kv;
    return kv - (h->tableSize * (kv >= &h->table[h->tableSize]));
}

static uint64_t computeIndex(uint64_t hash, uint64_t tableSize) {
    typedef unsigned __int128 u128;
    return (uint64_t) ((u128) hash * (u128) tableSize >> 64);
}

static uint64_t probeLength(uint64_t index1, uint64_t index2, uint64_t tableSize) {
    if (index1 <= index2)
        return index2 - index1;
    else
        return tableSize - (index1 - index2);
}

static uint64_t span(axhashmap *h, const void *key) {
    if (h->staticSpan)
        return h->staticSpan;
    else
        return h->dynamicSpan(key);
}

static bool matches(axhashmap *h, const KeyValue *kv1, const KeyValue *kv2) {
    if (kv1->hash != kv2->hash)
        return false;
    else if (h->staticSpan)
        return memcmp(kv1->key, kv2->key, h->staticSpan) == 0;
    else if (h->dynamicSpan == strlenSpan)
        return strcmp(kv1->key, kv2->key);
    else {
        uint64_t len1 = h->dynamicSpan(kv1->key);
        uint64_t len2 = h->dynamicSpan(kv2->key);
        return len1 == len2 && memcmp(kv1->key, kv2->key, len1) == 0;
    }
}

static bool crowded(axhashmap *h) {
    return h->size >= h->rehashThreshold;
}

static uint64_t nextTableSize(axhashmap *h) {
    return h->tableSize * 2;
}


uint64_t axh_size(axhashmap *h) {
    return h->size;
}

uint64_t axh_tableSize(axhashmap *h) {
    return h->tableSize;
}

axhashmap *axh_setLoadFactor(axhashmap *h, double lf) {
    if (lf < 0) lf = 0;
    if (lf > 1) lf = 1;
    h->loadFactor = lf;
    h->rehashThreshold = (uint64_t) ((double) h->tableSize * lf);
    return h;
}

double axh_getLoadFactor(axhashmap *h) {
    return h->loadFactor;
}

axhashmap *axh_setDynamicSpan(axhashmap *h, uint64_t (*dynamicSpan)(const void *)) {
    h->dynamicSpan = dynamicSpan ? dynamicSpan : strlenSpan;
    return h;
}

uint64_t (*axh_getDynamicSpan(axhashmap *h))(const void *) {
    return h->dynamicSpan;
}

axhashmap *axh_setDestructor(axhashmap *h, void (*destroy)(void *, void *)) {
    h->destroy = destroy;
    return h;
}

void (*axh_getDestructor(axhashmap *h))(void *, void *) {
    return h->destroy;
}


axhashmap *axh_sizedNew(uint64_t span, uint64_t size, double loadFactor) {
    size += !size;
    axhashmap *h = malloc(sizeof *h);

    if (!h || !(h->table = calloc(size, sizeof *h->table))) {
        free(h);
        return NULL;
    }

    h->loadFactor = loadFactor;
    h->rehashThreshold = (uint64_t) ((double) size * h->loadFactor);
    h->size = 0;
    h->tableSize = size;
    h->staticSpan = span;
    h->dynamicSpan = strlenSpan;
    h->destroy = NULL;
    return h;
}

axhashmap *axh_new(uint64_t span) {
    return axh_sizedNew(span, 16, 2./3.);
}

void axh_destroy(axhashmap *h) {
    if (h->destroy) {
        KeyValue *kv = h->table;
        for (uint64_t mapped = 0; mapped < h->size; ++kv) {
            if (!isEmpty(kv)) {
                h->destroy(kv->key, kv->value);
                ++mapped;
            }
        }
    }

    free(h->table);
    free(h);
}

static bool unsafeMap(axhashmap *h, KeyValue *kv) {
    uint64_t index = computeIndex(kv->hash, h->tableSize);
    KeyValue *selection = &h->table[index];

    for (uint64_t kvProbes = 0; !isEmpty(selection); ++kvProbes) {
        if (matches(h, selection, kv))
            return true;

        const uint64_t selectionProbes = probeLength(computeIndex(selection->hash, h->tableSize), index, h->tableSize);
        if (kvProbes > selectionProbes) {
            KeyValue tmp = *selection;
            *selection = *kv;
            *kv = tmp;
            kvProbes = selectionProbes;
        }

        index = mod1(index + 1, h->tableSize);
        selection = &h->table[index];
    }

    *selection = *kv;
    ++h->size;
    return false;
}

bool axh_rehash(axhashmap *h, uint64_t size) {
    axhashmap h2 = {NULL, .0, 0, 0, size, h->staticSpan, h->dynamicSpan, NULL};
    if (size < h->size)
        return true;
    if (!(h2.table = calloc(size, sizeof *h2.table)))
        return true;

    for (uint64_t i = 0, mapped = 0; mapped < h->size; ++i) {
        KeyValue *selection = &h->table[i];

        if (!isEmpty(selection)) {
            unsafeMap(&h2, selection);
            ++mapped;
        }
    }

    free(h->table);
    h->table = h2.table;
    h->tableSize = size;
    h->rehashThreshold = (uint64_t) ((double) size * h->loadFactor);
    return false;
}

int axh_sizedMap(axhashmap *h, void *key, void *value, uint64_t size) {
    if (crowded(h) && axh_rehash(h, nextTableSize(h)))
        return -1;

    KeyValue kv = {XXH3_64bits(key, size), key, value};
    return unsafeMap(h, &kv);
}

int axh_map(axhashmap *h, void *key, void *value) {
    return axh_sizedMap(h, key, value, span(h, key));
}

int axh_add(axhashmap *h, void *key) {
    return axh_sizedMap(h, key, key, span(h, key));
}

static KeyValue *locate(axhashmap *h, void *key) {
    XXH64_hash_t hash = XXH3_64bits(key, span(h, key));
    uint64_t index = computeIndex(hash, h->tableSize);
    const KeyValue kv = {hash, key, NULL};
    KeyValue *selection = &h->table[index];

    for (uint64_t kvProbes = 0; !isEmpty(selection); ++kvProbes) {
        if (matches(h, selection, &kv))
            return selection;

        uint64_t selectionIndex = computeIndex(selection->hash, h->tableSize);
        if (kvProbes > probeLength(selectionIndex, index, h->tableSize))
            return NULL;
        
        index = mod1(index + 1, h->tableSize);
        selection = &h->table[index];
    }

    return NULL;
}

bool axh_has(axhashmap *h, void *key) {
    return locate(h, key);
}

void *axh_get(axhashmap *h, void *key) {
    KeyValue *kv = locate(h, key);
    return kv ? kv->value : NULL;
}

bool axh_remove(axhashmap *h, void *key) {
    KeyValue *selection = locate(h, key);
    if (!selection)
        return false;
    
    uint64_t index = selection - h->table;
    KeyValue *prior = selection;
    selection = nextKV(h, selection);

    if (h->destroy)
        h->destroy(prior->key, prior->value);
    
    while (!isEmpty(selection)) {
        const uint64_t nextIndex = mod1(index + 1, h->tableSize);
        if (probeLength(computeIndex(selection->hash, h->tableSize), nextIndex, h->tableSize) == 0)
            break;
        
        *prior = *selection;
        prior = selection;
        selection = nextKV(h, selection);
        index = nextIndex;
    }
    
    *prior = (KeyValue) {0};
    --h->size;
    return true;
}

