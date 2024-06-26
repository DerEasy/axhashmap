//
// Created by easy on 20.03.24.
//

/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


#include "axhashmap.h"
#include <string.h>
#include <stdlib.h>
#include <xxhash.h>


static void *(*malloc_)(size_t) = malloc;
static void *(*calloc_)(size_t, size_t) = calloc;
static void (*free_)(void *) = free;

typedef struct KeyValue {
    XXH64_hash_t hash;
    void *key;
    void *value;
} KeyValue;

struct axhashmap {
    KeyValue *table;
    uint64_t rehashThreshold;
    uint64_t size;
    uint64_t tableSize;
    uint64_t staticSpan;
    uint64_t (*toHash)(const void *, uint64_t (*)(const void *, size_t));
    bool (*cmp)(const void *, const void *);
    void (*destroy)(void *, void *);
    double loadFactor;
};


static bool isEmpty(const KeyValue *kv) {
    return !kv->hash && !kv->key;
}

static uint64_t strToHash(const void *str, uint64_t (*_)(const void *, size_t)) {
    (void) _;
    return XXH3_64bits(str, strlen(str));
}

static bool cmpAddresses(const void *a, const void *b) {
    return a == b;
}

static uint64_t mod1(uint64_t n, uint64_t k) {
    return n >= k ? n - k : n;
}

static KeyValue *nextKV(axhashmap *h, KeyValue *kv) {
    ++kv;
    return kv - (h->tableSize * (kv >= &h->table[h->tableSize]));
}

static uint64_t computeIndex(uint64_t hash, uint64_t tableSize) {
    /* Fast modulo reduction using multiplication overflow. Needs 128-bit support though. */
    __extension__ typedef  unsigned __int128 u128;
    return (uint64_t) ((u128) hash * (u128) tableSize >> 64);
}

static uint64_t probeLength(uint64_t index1, uint64_t index2, uint64_t tableSize) {
    if (index1 <= index2)
        return index2 - index1;
    else
        return tableSize - (index1 - index2);
}

static uint64_t hashKey(axhashmap *h, void *key) {
    if (h->staticSpan)
        return XXH3_64bits(key, h->staticSpan);
    else
        return h->toHash(key, XXH3_64bits);
}

static bool matches(axhashmap *h, const KeyValue *kv1, const KeyValue *kv2) {
    if (kv1->hash != kv2->hash)
        return false;
    else if (h->staticSpan)
        return memcmp(kv1->key, kv2->key, h->staticSpan) == 0;
    else if (h->toHash == strToHash && h->cmp == cmpAddresses)
        return strcmp(kv1->key, kv2->key) == 0;
    else
        return h->cmp(kv1->key, kv2->key);
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

uint64_t axh_span(axhashmap *h) {
    return h->staticSpan;
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

axhashmap *axh_setComparator(axhashmap *h, bool (*cmp)(const void *, const void *)) {
    h->cmp = cmp ? cmp : cmpAddresses;
    return h;
}

bool (*axh_getComparator(axhashmap *h))(const void *, const void *) {
    return h->cmp;
}

axhashmap *axh_setToHash(axhashmap *h, uint64_t (*toHash)(const void *, uint64_t (*)(const void *, size_t))) {
    h->toHash = toHash ? toHash : strToHash;
    return h;
}

uint64_t (*axh_getToHash(axhashmap *h))(const void *, uint64_t (*)(const void *, size_t)) {
    return h->toHash;
}

axhashmap *axh_setDestructor(axhashmap *h, void (*destroy)(void *, void *)) {
    h->destroy = destroy;
    return h;
}

void (*axh_getDestructor(axhashmap *h))(void *, void *) {
    return h->destroy;
}

void axh_memoryfn(void *(*malloc_fn)(size_t), void *(*calloc_fn)(size_t, size_t), void (*free_fn)(void *)) {
    malloc_ = malloc_fn ? malloc_fn : malloc;
    calloc_ = calloc_fn ? calloc_fn : calloc;
    free_ = free_fn ? free_fn : free;
}


axhashmap *axh_newSized(uint64_t span, uint64_t tableSize, double loadFactor) {
    tableSize += !tableSize;
    axhashmap *h = malloc_(sizeof *h);
    if (!h || !(h->table = calloc_(tableSize, sizeof *h->table))) {
        free_(h);
        return NULL;
    }
    h->rehashThreshold = (uint64_t) ((double) tableSize * loadFactor);
    h->size = 0;
    h->tableSize = tableSize;
    h->staticSpan = span;
    h->toHash = strToHash;
    h->cmp = cmpAddresses;
    h->destroy = NULL;
    h->loadFactor = loadFactor;
    return h;
}

axhashmap *axh_new(uint64_t span) {
    return axh_newSized(span, 16, 2./3.);
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
    free_(h->table);
    free_(h);
}

static bool unsafeMap(axhashmap *h, KeyValue *kv, const bool mightMatch, const bool remap) {
    uint64_t index = computeIndex(kv->hash, h->tableSize);
    KeyValue *selection = &h->table[index];
    for (uint64_t kvProbes = 0; !isEmpty(selection); ++kvProbes) {
        if (mightMatch && matches(h, selection, kv)) {
            if (remap) {
                void *value = selection->value;
                *selection = *kv;
                kv->value = value;
            }
            return true;
        }

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

bool axh_rehash(axhashmap *h, uint64_t tableSize) {
    axhashmap h2 = {.tableSize = tableSize};
    if (tableSize < h->size)
        return true;
    if (!(h2.table = calloc_(tableSize, sizeof *h2.table)))
        return true;
    for (uint64_t i = 0, mapped = 0; mapped < h->size; ++i) {
        KeyValue *selection = &h->table[i];
        if (!isEmpty(selection)) {
            unsafeMap(&h2, selection, false, false);
            ++mapped;
        }
    }
    free_(h->table);
    h->table = h2.table;
    h->tableSize = tableSize;
    h->rehashThreshold = (uint64_t) ((double) tableSize * h->loadFactor);
    return false;
}

int axh_map(axhashmap *h, void *key, void *value) {
    if (crowded(h) && axh_rehash(h, nextTableSize(h)))
        return -1;
    KeyValue kv = {hashKey(h, key), key, value};
    return unsafeMap(h, &kv, true, false);
}

int axh_remap(axhashmap *h, void *key, void *value) {
    if (crowded(h) && axh_rehash(h, nextTableSize(h)))
        return -1;
    KeyValue kv = {hashKey(h, key), key, value};
    bool status = unsafeMap(h, &kv, true, true);
    if (status && h->destroy)
        h->destroy(NULL, kv.value);
    return status;
}

int axh_add(axhashmap *h, void *key) {
    return axh_map(h, key, key);
}

static KeyValue *locate(axhashmap *h, void *key) {
    XXH64_hash_t hash = hashKey(h, key);
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

bool axh_tryGet(axhashmap *h, void *key, void *value) {
    KeyValue *kv = locate(h, key);
    if (kv)
        *(void **) value = kv->value;
    return kv;
}

static void unsafeUnmap(axhashmap *h, KeyValue *selection) {
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
}

bool axh_unmap(axhashmap *h, void *key) {
    KeyValue *selection = locate(h, key);
    if (selection)
        unsafeUnmap(h, selection);
    return selection;
}

axhashmap *axh_filter(axhashmap *h, bool (*f)(const void *, void *, void *), void *arg) {
    KeyValue *kv = h->table;
    for (uint64_t passed = 0; passed < h->size; ++kv) {
        const bool alive = !isEmpty(kv);
        passed += alive;
        if (alive && f(kv->key, kv->value, arg))
            unsafeUnmap(h, kv);
    }
    return h;
}

axhashmap *axh_foreach(axhashmap *h, bool (*f)(const void *, void *, void *), void *arg) {
    KeyValue *kv = h->table;
    for (uint64_t passed = 0; passed < h->size; ++kv) {
        const bool alive = !isEmpty(kv);
        passed += alive;
        if (alive && !f(kv->key, kv->value, arg))
            return h;
    }
    return h;
}

axhashmap *axh_clear(axhashmap *h) {
    KeyValue *kv = h->table;
    if (h->destroy) {
        for (uint64_t unmapped = 0; unmapped < h->size; *kv++ = (KeyValue) {0}) {
            if (!isEmpty(kv)) {
                h->destroy(kv->key, kv->value);
                ++unmapped;
            }
        }
    }
    h->size = 0;
    memset(kv, 0, (&h->table[h->tableSize] - kv) * sizeof *h->table);
    return h;
}

axhashmap *axh_copy(axhashmap *h) {
    axhashmap *h2 = malloc_(sizeof *h2);
    if (!h2 || !(h2->table = malloc_(h->tableSize * sizeof *h->table))) {
        free_(h2);
        return NULL;
    }
    memcpy(h2->table, h->table, h->tableSize * sizeof *h->table);
    h2->rehashThreshold = h->rehashThreshold;
    h2->size = h->size;
    h2->tableSize = h->tableSize;
    h2->staticSpan = h->staticSpan;
    h2->toHash = h->toHash;
    h2->cmp = h->cmp;
    h2->destroy = NULL;
    h2->loadFactor = h->loadFactor;
    return h2;
}

