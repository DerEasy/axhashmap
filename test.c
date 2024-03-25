//
// Created by easy on 22.03.24.
//

#include "axhashmap.h"
#include <stdlib.h>
#include <stdio.h>
#include <xoshiro256starstar.h>
#include <sys/random.h>
#include <assert.h>
#include <string.h>


static void shuffle(uint64_t *xs, size_t n, struct xsr256ss *seed) {
    for (size_t i = 0; i < n - 1; ++i) {
        size_t j = i + xsr256ss(seed) % (n - i);
        uint64_t tmp = xs[i];
        xs[i] = xs[j];
        xs[j] = tmp;
    }
}

void testRemove(struct xsr256ss *seed) {
    puts("Testing remove...");
    enum {N = 1000};
    uint64_t pool[N];
    uint64_t tmp[N];

    for (int trials = 0; trials < 1000; ++trials) {
        unsigned removeCount = xsr256ss(seed) % (N + 1);
        axhashmap *h = axh_new(sizeof(uint64_t));

        for (int i = 0; i < N; ++i)
            pool[i] = xsr256ss(seed);
        for (int i = 0; i < N; ++i)
            axh_add(h, &pool[i]);

        memcpy(tmp, pool, N * sizeof *tmp);
        shuffle(tmp, N, seed);
        for (unsigned i = 0; i < removeCount; ++i) {
            assert(axh_has(h, &tmp[i]));
            axh_remove(h, &tmp[i]);
            assert(!axh_has(h, &tmp[i]));
        }
        for (unsigned i = 0; i < N; ++i) {
            if (i < removeCount)
                assert(!axh_has(h, &tmp[i]));
            else
                assert(axh_has(h, &tmp[i]));
        }

        axh_destroy(h);
    }

    puts("Remove successful.");
}


int main(void) {
    struct xsr256ss seed;
    getrandom(&seed, sizeof seed, 0);
    testRemove(&seed);
}
