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


static void shuffleU64(uint64_t *xs, size_t n, struct xsr256ss *seed) {
    for (size_t i = 0; i < n - 1; ++i) {
        size_t j = i + xsr256ss(seed) % (n - i);
        uint64_t tmp = xs[i];
        xs[i] = xs[j];
        xs[j] = tmp;
    }
}

static void shuffleStrings(char **xs, size_t n, struct xsr256ss *seed) {
    for (size_t i = 0; i < n - 1; ++i) {
        size_t j = i + xsr256ss(seed) % (n - i);
        char *tmp = xs[i];
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
        shuffleU64(tmp, N, seed);
        for (unsigned i = 0; i < removeCount; ++i) {
            assert(axh_has(h, &tmp[i]));
            axh_unmap(h, &tmp[i]);
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

    puts("Removal successful.");
}


/* NOTICE: This test is probabilistic and will fail
   frequently if MINLEN is not sufficiently high. */
void testStrings(struct xsr256ss *seed) {
    puts("Testing native string functionality...");
    enum {N = 1000, ALLOCATED = 32, MINLEN = 16};
    char *pool[N];
    char *tmp[N];

    for (int i = 0; i < N; ++i)
        pool[i] = malloc(ALLOCATED);
    memcpy(tmp, pool, N * sizeof *tmp);

    for (int trials = 0; trials < 1000; ++trials) {
        axhashmap *h = axh_new(0);

        for (unsigned s = 0; s < N; ++s) {
            uint64_t length = MINLEN + xsr256ss(seed) % (ALLOCATED - MINLEN);
            for (uint64_t i = 0; i < length; ++i)
                pool[s][i] = (char) ('a' + xsr256ss(seed) % 26);
            pool[s][length] = '\0';

            assert(!axh_has(h, pool[s]));
            axh_add(h, pool[s]);
            assert(axh_has(h, pool[s]));
        }

        for (unsigned s = 0; s < N; ++s)
            assert(axh_has(h, pool[s]));

        unsigned removeCount = xsr256ss(seed) % (N + 1);
        shuffleStrings(tmp, N, seed);
        for (unsigned s = 0; s < removeCount; ++s) {
            assert(axh_has(h, tmp[s]));
            axh_unmap(h, tmp[s]);
            assert(!axh_has(h, tmp[s]));
        }

        for (unsigned s = 0; s < N; ++s) {
            if (s < removeCount)
                assert(!axh_has(h, tmp[s]));
            else
                assert(axh_has(h, tmp[s]));
        }

        axh_destroy(h);
    }

    for (int i = 0; i < N; ++i)
        free(pool[i]);
    puts("Native string functionality test successful");
}


void playground1(void) {
    axhashmap *h = axh_new(0);
    char *keys[] = {"+", "-", "&&", "||", "=="};
    enum {N = sizeof keys / sizeof *keys};
    char *values[N] = {"TOK_PLUS", "TOK_MINUS", "TOK_LOGICAL_AND", "TOK_LOGICAL_OR", "TOK_EQUALS"};

    for (int i = 0; i < N; ++i)
        axh_map(h, keys[i], values[i]);
    for (int i = 0; i < N; ++i)
        puts(axh_get(h, keys[i]));

    printf("size %lu\n", axh_size(h));
    axh_clear(h);
    printf("size %lu\n", axh_size(h));

    for (int i = 0; i < N; ++i)
        puts(axh_has(h, keys[i]) ? "yes" : "no");
    axh_destroy(h);
}


void playground2(void) {
    axhashmap *h = axh_new(0);
    char *keys[] = {"+", "-", "&&", "||", "=="};
    enum {N = sizeof keys / sizeof *keys};
    char *values[N] = {"TOK_PLUS", "TOK_MINUS", "TOK_LOGICAL_AND", "TOK_LOGICAL_OR", "TOK_EQUALS"};

    for (int i = 0; i < N; ++i)
        axh_map(h, keys[i], values[i]);
    axh_destroy(h);
}


void speedtest(struct xsr256ss *seed) {
    axhashmap *h = axh_sizedNew(sizeof(uint64_t), 105300000, 19./20.);
    for (uint64_t i = 0; i < 100000000; ++i) {
        axh_add(h, &i);
    }
    //axh_destroy(h);
}


int main(void) {
    struct xsr256ss seed;
    (void) getrandom(&seed, sizeof seed, 0);
    speedtest(&seed);
    /*playground1();
    testRemove(&seed);
    testStrings(&seed);*/
}
