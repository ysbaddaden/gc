// MurmurHash2 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.
//
// Note - This code makes a few assumptions about how your machine behaves:
//
// 1. We can read a 4-byte value from any address without crashing
// 2. sizeof(int) == 4
//
// And it has a few limitations:
//
// 1. It will not work incrementally
// 2. It will not produce the same results on little-endian and big-endian
//    machines

#ifndef MURMUR_H
#define MURMUR_H

#include <stdint.h>

static uint32_t MurmurHash2(const void *key, int len, uint32_t seed) {
    // 'm' and 'r' are mixing constants generated offline.
    // they're not really 'magic', they just happen to work well.
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    // initialize the hash to a 'random' value
    uint32_t h = seed ^ len;

    // mix 4 bytes at a time into the hash
    const unsigned char *data = (const unsigned char *)key;

    while (len >= 4) {
        uint32_t k = *(uint32_t *)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    // handle the last few bytes of the input array
    switch (len) {
        case 3:
            h ^= data[2] << 16;
            __attribute__ ((fallthrough));
        case 2:
            h ^= data[1] << 8;
            __attribute__ ((fallthrough));
        case 1:
            h ^= data[0];
            h *= m;
    };

    // do a few final mixes of the hash to ensure the last few bytes are
    // well-incorporated
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

#endif
