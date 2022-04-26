/*
Copyright 2020-2022 Esri
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
#include "QB3.h"
#include "bitstream.h"

// Define QB3_SHORT_TABLES to minimize the size of the tables
// This makes no difference for byte data, runs slower for large data types
// It saves about 20KB
// #define QB3_SHORT_TABLES
//
#include "QB3tables.h"

#if defined(_WIN32)
#include <intrin.h>
#endif

// Tables have 12bits of data, top 4 bits are size
#define TBLMASK 0xfffull

// Block is 4x4 pixels
constexpr size_t B(4);
constexpr size_t B2(B * B);

#if defined(__GNUC__) && defined(__x86_64__)
// Comment out if binary is to run on processors without sse4
#pragma GCC target("sse4")
#endif

#if defined(_WIN32)
// rank of top set bit, result is undefined for val == 0
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

static size_t setbits16(uint64_t val) {
    return nbits(0xff & val) + nbits(0xff & (val >> 8));
}

#endif

// Encoder control structure
struct encs {
    size_t xsize;
    size_t ysize;
    size_t nbands;
    size_t quanta;
    // band which will be subtracted, by band
    size_t cband[QB3_MAXBANDS];

    // end state
    size_t prev[QB3_MAXBANDS];
    size_t runbits[QB3_MAXBANDS];

    qb3_mode mode;
    qb3_dtype type;
    int error; // Holds the code for error, 0 if everything is fine
    bool away; // Round up instead of down when quantizing
    bool raw;  // Write QB3 raw stream
};

// Decoder control structure
struct decs {
    size_t xsize;
    size_t ysize;
    size_t nbands;
    size_t quanta;
    // band which will be subtracted, by band
    size_t cband[QB3_MAXBANDS];
    qb3_mode mode;
    qb3_dtype type;
    int error;
    bool raw;
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

// Absolute from mag-sign
template<typename T>
static T magsabs(T val) {
    return (val >> 1) + (val & 1);
}

// If the rung bits of the input values match 1*0*, returns the index of first 0, otherwise B2 + 1
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
