//
// Created by easy on 20.03.24.
//

/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef AXHASH_AXHASHMAP_H
#define AXHASH_AXHASHMAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Default load factor. */
#define AXH_LOADFACTOR (2./3.)

/*
 * axhashmap is a hashmap library using the Robin-Hood hashing technique with backward shifting and simple
 * linear lookup. The hashing function used is xxhash (XXH3).
 *
 * axhashmap has a concept of span. To create a map, you specify a span. This static span tells the map how many bytes
 * of a key to use for hashing. To compare mappings for equality, we compare their hashes. This is always done first,
 * regardless of which span mode is in use. If the hashes match, we compare up to span-many bytes of raw memory of
 * whatever their keys point to.
 *
 * Setting the static span to 0 activates dynamic span mode. If the toHash() and comparator functions are left at
 * their respective defaults, this mode can instantly be used for hashing null-terminated C strings and comparison
 * is done using strcmp(). If however toHash() has been set to some custom function, the default comparator
 * switches back to a mere address equality check. On the other hand, setting a custom comparator does not alter
 * the default toHash() behaviour of hashing a C string, since it can still be useful for i.e. UTF-8 strings.
 * Of course, setting both to custom functions renders these points moot.
 *
 * axhashmap supports destructors. There is no default destructor. Destructors have type void (*)(void *, void *)
 * with the first parameter being the key and the second being the value. The destructor is passed both in all cases
 * with the sole exception of axh_remap(), in which case only the value is passed and the key is NULL. Make your
 * destructor able to handle this case if you want to use the remap function alongside a destructor.
 *
 * Below are some results of testing different table loads and their performance.

            Table size of experiment: 1000000
            Hashed value type: 32-bit little endian signed integers
            Actual values mapped: 0 to (load * table size) [incl./excl.]

            Load            Average probe length            Probe length distribution of load factor 2/3:
            <1/6            negligible                      # 0 = 47.43% 316197
            1/6             1/10                            # 1 = 27.35% 182326
            1/5             1/8                             # 2 = 13.41% 89389
            1/4             1/6                             # 3 =  6.32% 42106
            1/3             1/4                             # 4 =  2.90% 19329
            2/5             1/3                             # 5 =  1.37% 9163
            1/2             1/2                             # 6 =  0.65% 4336
            3/5             3/4                             # 7 =  0.31% 2058
            2/3             1          (default)            # 8 =  0.14% 920
            7/10            7/6                             # 9 =  0.07% 448
            3/4             3/2                             #10 =  0.03% 213
            4/5             2                               #11 =  0.01% 99
            257/300         3                               #12 =  0.01% 50
            8/9             4                               #13 =  0.00% 22
            9/10            9/2                             #14 =  0.00% 8
            19/20           19/2                            #15 =  0.00% 2
            >19/20          vain
 */
typedef struct axhashmap axhashmap;


/**
 * Number of mappings in this map.
 * @return Size.
 */
uint64_t axh_size(axhashmap *h);

/**
 * Maximum number of allowed mappings in this map, disregarding load factor.
 * @return Size of table.
 */
uint64_t axh_tableSize(axhashmap *h);

/**
 * The static span of this map. The static span denotes how many bytes of a key are processed by the hash function.
 * A value of 0 means dynamic span mode is used.
 * @return Static span of self.
 */
uint64_t axh_span(axhashmap *h);

/**
 * Set load factor, which must be in range 0.0 to 1.0. Any other value is saturated.
 * This function does not ever automatically rehash. Rehashing happens when it is
 * needed or manually induced in some following operation.
 * @param lf Load factor from 0.0 to 1.0.
 * @return Self.
 */
axhashmap *axh_setLoadFactor(axhashmap *h, double lf);

/**
 * Currently set load factor.
 * @return Load factor from 0.0 to 1.0.
 */
double axh_getLoadFactor(axhashmap *h);

/**
 * Set comparator function. When two mappings' hashes are equal in dynamic span mode, this function is used
 * to determine their actual equality by passing it the keys. If toHash() is set to its default, the default comparator
 * is strcmp() (wrapped to conform to the prototype), otherwise it is a simple address equality check.
 * @param cmp Some comparator function or NULL for the default.
 * @return Self.
 */
axhashmap *axh_setComparator(axhashmap *h, bool (*cmp)(const void *, const void *));

/**
 * Get the currently used comparator function. The actual return value is unspecified whenever
 * the default comparator is active and shall not be relied upon in this case.
 * @return Comparator function of self.
 */
bool (*axh_getComparator(axhashmap *h))(const void *, const void *);

/**
 * Set toHash() function. The hashmap uses this function to pass a key and its hashing function so the
 * user may compute the hash themselves however they see fit in dynamic span mode. toHash() shall return
 * the hash to be used in the table. The default toHash() function hashes a null-terminated C string.
 * @param toHash Some function satisfying toHash()'s requirements or NULL for the default.
 * @return Self.
 */
axhashmap *axh_setToHash(axhashmap *h, uint64_t (*toHash)(const void *, uint64_t (*)(const void *, size_t)));

/**
 * Get the currently used toHash() function.
 * @return toHash() function of self.
 */
uint64_t (*axh_getToHash(axhashmap *h))(const void *, uint64_t (*)(const void *, size_t));

/**
 * Set a destructor function to be called on unmapped items.
 * @param destroy Destructor function taking (key, value) or NULL to disable.
 * @return Self.
 */
axhashmap *axh_setDestructor(axhashmap *h, void (*destroy)(void *, void *));

/**
 * Get the currently used destructor function.
 * @return Destructor function or NULL if none is set.
 */
void (*axh_getDestructor(axhashmap *h))(void *, void *);

/**
 * Set custom memory functions. All three of them must be set and be compatible with one another. Passing NULL for any
 * function will activate its standard library counterpart.
 * @param malloc_fn The malloc function.
 * @param calloc_fn The calloc function.
 * @param free_fn The free function.
 */
void axh_memoryfn(void *(*malloc_fn)(size_t), void *(*calloc_fn)(size_t, size_t), void (*free_fn)(void *));


/**
 * Create a new hashmap with some custom table size, load factor and span.
 * @param span Span of keys.
 * @param tableSize Maximum number of allowed mappings, disregarding load factor.
 * @param loadFactor Load factor.
 * @return New hashmap or NULL iff OOM.
 */
axhashmap *axh_newSized(uint64_t span, uint64_t tableSize, double loadFactor);

/**
 * Create a new hashmap with default table size and load factor and custom span.
 * @param span Span of keys.
 * @return New hashmap or NULL iff OOM.
 */
axhashmap *axh_new(uint64_t span);

/**
 * Destroy all mappings if a destructor is available, then free the hashmap.
 */
void axh_destroy(axhashmap *h);

/**
 * Rehash the map with some table size. This function always allocates another table.
 * This function does not resize when the table is overloaded, but it does fail if
 * the new table size is unable to hold all mappings.
 * @param tableSize Size of newly allocated table.
 * @return True if OOM or given table size is less than the number of mappings, else false.
 */
bool axh_rehash(axhashmap *h, uint64_t tableSize);

/**
 * Map a key to some value if it does not already exist.
 * @param key Key.
 * @param value Value.
 * @return -1 if OOM, 0 if a new mapping was created, 1 if the mapping already exists.
 */
int axh_map(axhashmap *h, void *key, void *value);

/**
 * Map a key to some value unconditionally. That is, a new mapping is either created
 * or an existing matching mapping is replaced. In that case, the destructor (if set)
 * is called with only the value and NULL in place of the key.
 * @param key Key.
 * @param value Value.
 * @return -1 if OOM, 0 if a new mapping was created, 1 if an existing mapping was replaced.
 */
int axh_remap(axhashmap *h, void *key, void *value);

/**
 * Convenience function for sets. Calls axh_map() with the key and value being equal.
 * @param key Key and value in one.
 * @return -1 if OOM, 0 if a new mapping was created, 1 if the mapping already exists.
 */
int axh_add(axhashmap *h, void *key);

/**
 * Checks if a mapping with this key exists.
 * @param key Key with which to search for the mapping.
 * @return True iff a matching mapping was found.
 */
bool axh_has(axhashmap *h, void *key);

/**
 * Get the value of a mapping.
 * @param key Key with which to search for the mapping.
 * @return The value or NULL if no mapping was found.
 */
void *axh_get(axhashmap *h, void *key);

/**
 * Try getting the value of a mapping.
 * @param key Key with which to search for the mapping.
 * @param value Pointer to where the value will be written if a matching mapping is found. Nothing is done
 * otherwise.
 * @return True iff a matching mapping was found.
 */
bool axh_tryGet(axhashmap *h, void *key, void *value);

/**
 * Unmap a mapping if it exists and call the destructor if it is available.
 * @param key Key with which to search for the mapping.
 * @return True iff a matching mapping was found and unmapped.
 */
bool axh_unmap(axhashmap *h, void *key);

/**
 * Let f be a predicate taking (key, value, optional argument).
 * Any mapping which does not satisfy the predicate f shall subsequently be unmapped.
 * Unmapped mappings are destroyed if a destructor is available.
 * @param f A function acting as the predicate for the filter.
 * @param arg Some optional argument that is passed to f.
 * @return Self.
 */
axhashmap *axh_filter(axhashmap *h, bool (*f)(const void *, void *, void *), void *arg);

/**
 * Let f be a predicate taking (key, value, optional argument).
 * All mappings are sequentially passed to f in some unspecified order until
 * either the table is exhausted or f returns false.
 * @param f A function acting as the predicate for the loop.
 * @param arg Some optional argument that is passed to f.
 * @return Self.
 */
axhashmap *axh_foreach(axhashmap *h, bool (*f)(const void *, void *, void *), void *arg);

/**
 * Unmap all mappings and destroy them if a destructor is available.
 * @return Self.
 */
axhashmap *axh_clear(axhashmap *h);

/**
 * Create an exact copy of a hashmap. That is, all mappings are merely copied to the
 * new hashmap the way they appear in memory instead of being properly rehashed and mapped.
 * Beware that the destructor is not copied along.
 * @return Copy of this hashmap or NULL iff OOM.
 */
axhashmap *axh_copy(axhashmap *h);

#endif //AXHASH_AXHASHMAP_H
