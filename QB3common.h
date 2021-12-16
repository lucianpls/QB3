/*
Copyright 2020-2021 Esri
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Contributors:  Lucian Plesea

Content: QB3 parts used by both the encoder and the decoder

*/

#pragma once

#if defined(_WIN32)
#include <intrin.h>
#endif

// Block is 4x4 pixels
constexpr size_t B(4);
constexpr size_t B2(B * B);

#include "qb3_tables.h"

#if defined(__GNUC__) && defined(__x86_64__)
// Comment out if binary is to run on processors without sse4
#pragma GCC target("sse4")
#endif

#if defined(_WIN32)
// rank of top set bit, result is undefined for val == 0
static size_t topbit(uint64_t val) {
    return 63 - __lzcnt64(val);
}

static size_t setbits(uint64_t val) {
    return __popcnt64(val);
}

static size_t setbits16(uint64_t val) {
    return __popcnt64(val);
}

#elif defined(__GNUC__)
static size_t topbit(uint64_t val) {
    return 63 - __builtin_clzll(val);
}

static size_t setbits(uint64_t val) {
    return __builtin_popcountll(val);
}

static size_t setbits16(uint64_t val) {
    return setbits(val);
}

#else // no builtins, portable C
static size_t topbit(uint64_t val) {
    unsigned r = 0;
    while (val >>= 1)
        r++;
    return r;
}

// My own portable byte bitcount
static inline size_t nbits(uint8_t v) {
    return ((((v - ((v >> 1) & 0x55u)) * 0x1010101u) & 0x30c00c03u) * 0x10040041u) >> 0x1cu;
}

static size_t setbits(uint64_t val) {
    return nbits(0xff & val) + nbits(0xff & (val >> 8))
        + nbits(0xff & (val >> 16)) + nbits(0xff & (val >> 24))
        + nbits(0xff & (val >> 32)) + nbits(0xff & (val >> 40))
        + nbits(0xff & (val >> 48)) + nbits(0xff & (val >> 56));
}

static size_t setbits16(uint64_t val) {
    return nbits(0xff & val) + nbits(0xff & (val >> 8));
}
#endif

// Encode integers as absolute magnitude and sign, so the bit 0 is the sign.
// This encoding has the top rung always zero, regardless of sign
// To keep the range exactly the same as two's complement, the magnitude of 
// negative values is biased down by one (no negative zero)

// Change to mag-sign without conditionals, as fast as C can make it
template<typename T>
static T mags(T v) {
    return (std::numeric_limits<T>::max() * (v >> (8 * sizeof(T) - 1))) ^ (v << 1);
}

// Undo mag-sign without conditionals, as fast as C can make it
template<typename T>
static T smag(T v) {
    return (std::numeric_limits<T>::max() * (v & 1)) ^ (v >> 1);
}

// Convert from mag-sign to absolute
template<typename T>
static inline T magsabs(T val) {
    return (val >> 1) + (val & 1);
}

// If the rung bits of the input values match *1*0, returns the index of first 0, otherwise B2 + 1
template<typename T>
static size_t step(const T* const v, size_t rung) {
    // We are looking for 1*0* pattern on the B2 bits
    uint64_t acc = ~0ull;
    for (size_t i = 0; i < B2; i++)
        acc = (acc << 1) | (1 & (v[i] >> rung));
    // Flip bits so the top ones are 0 and bottom ones are 1
    acc = ~acc;
    auto s = setbits16(acc);
    if (0 == s) // don't call topbit if acc is empty
        return B2;
    if (topbit(acc) != s - 1)
        return B2 + 1;
    return B2 - s;
}
