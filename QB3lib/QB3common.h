/*
Content: QB3 parts used by both the encoder and the decoder

Copyright 2020-2023 Esri
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
*/

#pragma once
#include "QB3.h"
#include "bitstream.h"
#include <cinttypes>
#include <utility>
#include <type_traits>

#if defined(_WIN32)
#include <intrin.h>
#endif

// Tables have 12bits of data, top 4 bits are size
constexpr auto TBLMASK(0xfffull);

// Block is 4x4 pixels
constexpr size_t B(4);
constexpr size_t B2(B * B);

#if defined(__GNUC__) && defined(__x86_64__)
// Comment out if binary is to run on processors without sse4
#pragma GCC target("sse4")
#endif

#if defined(_WIN32)
// blog2 of val, result is undefined for val == 0
static size_t topbit(uint64_t val) {
    return 63 - __lzcnt64(val);
}

static size_t setbits16(uint64_t val) {
    return __popcnt64(val);
}

#elif defined(__GNUC__)
static size_t topbit(uint64_t val) {
    return 63 - __builtin_clzll(val);
}

static size_t setbits16(uint64_t val) {
    return __builtin_popcountll(val);
}

#else // no builtins, portable C
// blog2 of val, result is undefined for val == 0
static size_t topbit(uint64_t v) {
    size_t r, t;
    r = size_t(0 != (v >> 32)) << 5; v >>= r;
    t = size_t(0 != (v >> 16)) << 4; v >>= t; r |= t;
    t = size_t(0 != (v >> 8)) << 3;  v >>= t; r |= t;
    t = size_t(0 != (v >> 4)) << 2;  v >>= t; r |= t;
    return r + ((0xffffaa50ull >> (v << 1)) & 0x3);
}

// My own portable byte bitcount
static inline size_t nbits(uint8_t v) {
    return ((((v - ((v >> 1) & 0x55u)) * 0x1010101u) & 0x30c00c03u) * 0x10040041u) >> 0x1cu;
}

static size_t setbits16(uint64_t val) {
    return nbits(0xff & val) + nbits(0xff & (val >> 8));
}

#endif

struct band_state {
    size_t prev, runbits, cf;
};

// Encoder control structure
struct encs {
    size_t xsize;
    size_t ysize;
    size_t nbands;
    size_t quanta;

    // Persistent stage by band
    band_state band[QB3_MAXBANDS];
    // band which will be subtracted, by band
    size_t cband[QB3_MAXBANDS];

    int error; // Holds the code for error, 0 if everything is fine

    qb3_mode mode;
    qb3_dtype type;
    bool away; // Round up instead of down when quantizing
};

// Decoder control structure
struct decs {
    size_t xsize;
    size_t ysize;
    size_t nbands;
    size_t quanta;
    int error;
    int stage;

    // band which will be added, by band
    uint8_t cband[QB3_MAXBANDS];
    qb3_mode mode;
    qb3_dtype type;

    // Input buffer
    uint8_t* s_in;
    size_t s_size;
};

// in decode.cpp
extern const int typesizes[8];

// Encode integers as magnitude and sign, with bit 0 for sign.
// This encoding has the top bits always zero, regardless of sign
// To keep the range the same as two's complement, the magnitude of 
// negative values is biased down by one (no negative zero)

// Change to mag-sign without conditionals, as fast as C can make it
template<typename T>
static T mags(T v) {
    return (v << 1) ^ (~T(0) * (v >> (8 * sizeof(T) - 1)));
}

// Undo mag-sign without conditionals, as fast as C can make it
template<typename T>
static T smag(T v) {
    return (v >> 1) ^ (~T(0) * (v & 1));
}

// If the rung bits of the input values match 1*0*, returns the index of first 0, otherwise B2 + 1
template<typename T>
static size_t step(const T* const v, size_t rung) {
    uint64_t acc = 0ull;
    // Accumulate flipped rung bits
    for (size_t i = 0; i < B2; i++)
        acc = (acc << 1) | (1 ^ (v[i] >> rung));
    // pattern is now 0*1*, with at least one 1 set
    // s is 1 if distribution is a down step, 0 otherwise
    bool s = ((acc & (acc + 1)) != 0);
    return B2 + s - !s * setbits16(acc);
}
