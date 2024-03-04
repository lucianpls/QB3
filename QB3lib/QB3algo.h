/*
Content:
QB3 algorithm low level encoding and decoding 
QB3 is a lossless compression algorithm for 2D arrays of integers
This file, together with bitstream.h, can be used as a standalone C++ library

Copyright 2020-2024 Esri
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
#include "bitstream.h"
#include <cinttypes>
#include <utility>
#include <type_traits>
#include <algorithm>

#if !defined(QB3_MAXBANDS)
#define QB3_MAXBANDS 16
#endif

#if defined(_WIN32)
#include <intrin.h>
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
    t = size_t(0 != (v >>  8)) << 3; v >>= t; r |= t;
    t = size_t(0 != (v >>  4)) << 2; v >>= t; r |= t;
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

// Tables have 12bits of data, top 4 bits are size
constexpr auto TBLMASK(0xfffull);
// Block is fixed at 4x4 pixels
constexpr size_t B(4);
constexpr size_t B2(B * B);

struct band_state {
    size_t prev, runbits, cf;
};

// Encoder control structure
struct encs {
    size_t xsize, ysize, nbands;
    // micro block scanning order
    uint64_t order;

    // Persistent state by band
    band_state band[QB3_MAXBANDS];
    // band which will be subtracted, by band
    size_t cband[QB3_MAXBANDS];
    int error; // Holds the code for error, 0 if everything is fine

#if defined(QB3_CAPI)
    qb3_mode mode;
    qb3_dtype type;
    bool away; // Round up instead of down when quantizing
    size_t quanta;
#endif
};

// Decoder control structure
struct decs {
    size_t xsize, ysize, nbands;
    size_t stride; // Line to line offset
    uint64_t order; // micro block scanning curve
    int error;
    int stage;

    // band which will be added, by band
    uint8_t cband[QB3_MAXBANDS];

    // Input buffer
    uint8_t* s_in;
    size_t s_size;

#if defined(QB3_CAPI)
    qb3_mode mode;
    qb3_dtype type;
    size_t quanta;
#endif
};

// in decode.cpp

namespace QB3 {
// bytes per value by qb3_dtype, keep them in sync
const int typesizes[8] = { 1, 1, 2, 2, 4, 4, 8, 8 };

// First the encode

// Encode integers as magnitude and sign, with bit 0 for sign.
// This encoding has the top bits always zero, regardless of sign
// To keep the range the same as two's complement, the magnitude of 
// negative values is biased down by one (no negative zero)

// Change to mag-sign without conditionals, as fast as C can make it
template<typename T>
T mags(T v) { return (v << 1) ^ (~T(0) * (v >> (8 * sizeof(T) - 1))); }

// If the rung bits of the input values match 1*0*, returns the index of first 0, otherwise B2 + 1
template<typename T>
size_t step(const T* const v, size_t rung) {
    uint64_t acc = 0ull;
    // Accumulate flipped rung bits
    for (size_t i = 0; i < B2; i++)
        acc = (acc << 1) | (1 ^ (v[i] >> rung));
    // pattern is now 0*1*, with at least one 1 set
    // s is 1 if distribution is a down step, 0 otherwise
    bool s = ((acc & (acc + 1)) != 0);
    return B2 + s - !s * setbits16(acc);
}

// Two QB3 standard parsing order, encoded as a single 64bit value
// each nibble holds the adress of a pixel, two bits for x and two bits for y
// Use nibble values in the identity matrix, read in the desired order
/* Identity matrix, used to read the address
0 1 2 3
4 5 6 7
8 9 a b
c d e f
constexpr uint64_t ICURVE(0x0123456789abcdef);
*/

/* The legacy z-curve order
0 1 4 5
2 3 6 7
8 9 c d
a b e f
*/
constexpr uint64_t ZCURVE(0x0145236789cdabef);

/* Hilbert space filling curve second degree, results in better compression than Z-curve
0 1 e f
3 2 d c
4 7 8 b
5 6 9 a
*/
constexpr uint64_t HILBERT(0x01548cd9aefb7623);

// Encoding tables for rungs up to 8, for speedup. Rung 0 and 1 are special
// Storage is under 1K
// Tables could be generated for more rungs, but the speedup is not worth it
// See tables.py for how they are generated
static const uint16_t crg0[] = { 0x1000, 0x1001 };
static const uint16_t crg1[] = { 0x1000, 0x2001, 0x3003, 0x3007 };
static const uint16_t crg2[] = { 0x2000, 0x2002, 0x3001, 0x3005, 0x4003, 0x4007, 0x400b, 0x400f };
static const uint16_t crg3[] = { 0x3000, 0x3002, 0x3004, 0x3006, 0x4001, 0x4005, 0x4009, 0x400d, 0x5003, 0x5007, 0x500b, 0x500f,
0x5013, 0x5017, 0x501b, 0x501f };
static const uint16_t crg4[] = { 0x4000, 0x4002, 0x4004, 0x4006, 0x4008, 0x400a, 0x400c, 0x400e, 0x5001, 0x5005, 0x5009, 0x500d,
0x5011, 0x5015, 0x5019, 0x501d, 0x6003, 0x6007, 0x600b, 0x600f, 0x6013, 0x6017, 0x601b, 0x601f, 0x6023, 0x6027, 0x602b, 0x602f,
0x6033, 0x6037, 0x603b, 0x603f };
static const uint16_t crg5[] = { 0x5000, 0x5002, 0x5004, 0x5006, 0x5008, 0x500a, 0x500c, 0x500e, 0x5010, 0x5012, 0x5014, 0x5016,
0x5018, 0x501a, 0x501c, 0x501e, 0x6001, 0x6005, 0x6009, 0x600d, 0x6011, 0x6015, 0x6019, 0x601d, 0x6021, 0x6025, 0x6029, 0x602d,
0x6031, 0x6035, 0x6039, 0x603d, 0x7003, 0x7007, 0x700b, 0x700f, 0x7013, 0x7017, 0x701b, 0x701f, 0x7023, 0x7027, 0x702b, 0x702f,
0x7033, 0x7037, 0x703b, 0x703f, 0x7043, 0x7047, 0x704b, 0x704f, 0x7053, 0x7057, 0x705b, 0x705f, 0x7063, 0x7067, 0x706b, 0x706f,
0x7073, 0x7077, 0x707b, 0x707f };
static const uint16_t crg6[] = { 0x6000, 0x6002, 0x6004, 0x6006, 0x6008, 0x600a, 0x600c, 0x600e, 0x6010, 0x6012, 0x6014, 0x6016,
0x6018, 0x601a, 0x601c, 0x601e, 0x6020, 0x6022, 0x6024, 0x6026, 0x6028, 0x602a, 0x602c, 0x602e, 0x6030, 0x6032, 0x6034, 0x6036,
0x6038, 0x603a, 0x603c, 0x603e, 0x7001, 0x7005, 0x7009, 0x700d, 0x7011, 0x7015, 0x7019, 0x701d, 0x7021, 0x7025, 0x7029, 0x702d,
0x7031, 0x7035, 0x7039, 0x703d, 0x7041, 0x7045, 0x7049, 0x704d, 0x7051, 0x7055, 0x7059, 0x705d, 0x7061, 0x7065, 0x7069, 0x706d,
0x7071, 0x7075, 0x7079, 0x707d, 0x8003, 0x8007, 0x800b, 0x800f, 0x8013, 0x8017, 0x801b, 0x801f, 0x8023, 0x8027, 0x802b, 0x802f,
0x8033, 0x8037, 0x803b, 0x803f, 0x8043, 0x8047, 0x804b, 0x804f, 0x8053, 0x8057, 0x805b, 0x805f, 0x8063, 0x8067, 0x806b, 0x806f,
0x8073, 0x8077, 0x807b, 0x807f, 0x8083, 0x8087, 0x808b, 0x808f, 0x8093, 0x8097, 0x809b, 0x809f, 0x80a3, 0x80a7, 0x80ab, 0x80af,
0x80b3, 0x80b7, 0x80bb, 0x80bf, 0x80c3, 0x80c7, 0x80cb, 0x80cf, 0x80d3, 0x80d7, 0x80db, 0x80df, 0x80e3, 0x80e7, 0x80eb, 0x80ef,
0x80f3, 0x80f7, 0x80fb, 0x80ff };
static const uint16_t crg7[] = { 0x7000, 0x7002, 0x7004, 0x7006, 0x7008, 0x700a, 0x700c, 0x700e, 0x7010, 0x7012, 0x7014, 0x7016,
0x7018, 0x701a, 0x701c, 0x701e, 0x7020, 0x7022, 0x7024, 0x7026, 0x7028, 0x702a, 0x702c, 0x702e, 0x7030, 0x7032, 0x7034, 0x7036,
0x7038, 0x703a, 0x703c, 0x703e, 0x7040, 0x7042, 0x7044, 0x7046, 0x7048, 0x704a, 0x704c, 0x704e, 0x7050, 0x7052, 0x7054, 0x7056,
0x7058, 0x705a, 0x705c, 0x705e, 0x7060, 0x7062, 0x7064, 0x7066, 0x7068, 0x706a, 0x706c, 0x706e, 0x7070, 0x7072, 0x7074, 0x7076,
0x7078, 0x707a, 0x707c, 0x707e, 0x8001, 0x8005, 0x8009, 0x800d, 0x8011, 0x8015, 0x8019, 0x801d, 0x8021, 0x8025, 0x8029, 0x802d,
0x8031, 0x8035, 0x8039, 0x803d, 0x8041, 0x8045, 0x8049, 0x804d, 0x8051, 0x8055, 0x8059, 0x805d, 0x8061, 0x8065, 0x8069, 0x806d,
0x8071, 0x8075, 0x8079, 0x807d, 0x8081, 0x8085, 0x8089, 0x808d, 0x8091, 0x8095, 0x8099, 0x809d, 0x80a1, 0x80a5, 0x80a9, 0x80ad,
0x80b1, 0x80b5, 0x80b9, 0x80bd, 0x80c1, 0x80c5, 0x80c9, 0x80cd, 0x80d1, 0x80d5, 0x80d9, 0x80dd, 0x80e1, 0x80e5, 0x80e9, 0x80ed,
0x80f1, 0x80f5, 0x80f9, 0x80fd, 0x9003, 0x9007, 0x900b, 0x900f, 0x9013, 0x9017, 0x901b, 0x901f, 0x9023, 0x9027, 0x902b, 0x902f,
0x9033, 0x9037, 0x903b, 0x903f, 0x9043, 0x9047, 0x904b, 0x904f, 0x9053, 0x9057, 0x905b, 0x905f, 0x9063, 0x9067, 0x906b, 0x906f,
0x9073, 0x9077, 0x907b, 0x907f, 0x9083, 0x9087, 0x908b, 0x908f, 0x9093, 0x9097, 0x909b, 0x909f, 0x90a3, 0x90a7, 0x90ab, 0x90af,
0x90b3, 0x90b7, 0x90bb, 0x90bf, 0x90c3, 0x90c7, 0x90cb, 0x90cf, 0x90d3, 0x90d7, 0x90db, 0x90df, 0x90e3, 0x90e7, 0x90eb, 0x90ef,
0x90f3, 0x90f7, 0x90fb, 0x90ff, 0x9103, 0x9107, 0x910b, 0x910f, 0x9113, 0x9117, 0x911b, 0x911f, 0x9123, 0x9127, 0x912b, 0x912f,
0x9133, 0x9137, 0x913b, 0x913f, 0x9143, 0x9147, 0x914b, 0x914f, 0x9153, 0x9157, 0x915b, 0x915f, 0x9163, 0x9167, 0x916b, 0x916f,
0x9173, 0x9177, 0x917b, 0x917f, 0x9183, 0x9187, 0x918b, 0x918f, 0x9193, 0x9197, 0x919b, 0x919f, 0x91a3, 0x91a7, 0x91ab, 0x91af,
0x91b3, 0x91b7, 0x91bb, 0x91bf, 0x91c3, 0x91c7, 0x91cb, 0x91cf, 0x91d3, 0x91d7, 0x91db, 0x91df, 0x91e3, 0x91e7, 0x91eb, 0x91ef,
0x91f3, 0x91f7, 0x91fb, 0x91ff };

static const uint16_t* CRG[] = { crg0, crg1, crg2, crg3, crg4, crg5, crg6, crg7 };

// Code switch encoding tables, about 256 bytes, stored the same way
// See tables.py for how they are generated
// They are defined for 3 - 6 bits for unit length. 0x1000 means no change
static const uint16_t csw3[] = { 0x1000, 0x3001, 0x4003, 0x5007, 0x501f, 0x500f, 0x400b, 0x3005 };
static const uint16_t csw4[] = { 0x1000, 0x4001, 0x4009, 0x5003, 0x5013, 0x6007, 0x6017, 0x6027, 0x603f, 0x602f, 0x601f, 0x600f,
0x501b, 0x500b, 0x400d, 0x4005 };
static const uint16_t csw5[] = { 0x1000, 0x5001, 0x5009, 0x5011, 0x5019, 0x6003, 0x6013, 0x6023, 0x6033, 0x7007, 0x7017, 0x7027,
0x7037, 0x7047, 0x7057, 0x7067, 0x707f, 0x706f, 0x705f, 0x704f, 0x703f, 0x702f, 0x701f, 0x700f, 0x603b, 0x602b, 0x601b, 0x600b,
0x501d, 0x5015, 0x500d, 0x5005 };
static const uint16_t csw6[] = { 0x1000, 0x6001, 0x6009, 0x6011, 0x6019, 0x6021, 0x6029, 0x6031, 0x6039, 0x7003, 0x7013, 0x7023,
0x7033, 0x7043, 0x7053, 0x7063, 0x7073, 0x8007, 0x8017, 0x8027, 0x8037, 0x8047, 0x8057, 0x8067, 0x8077, 0x8087, 0x8097, 0x80a7,
0x80b7, 0x80c7, 0x80d7, 0x80e7, 0x80ff, 0x80ef, 0x80df, 0x80cf, 0x80bf, 0x80af, 0x809f, 0x808f, 0x807f, 0x806f, 0x805f, 0x804f,
0x803f, 0x802f, 0x801f, 0x800f, 0x707b, 0x706b, 0x705b, 0x704b, 0x703b, 0x702b, 0x701b, 0x700b, 0x603d, 0x6035, 0x602d, 0x6025,
0x601d, 0x6015, 0x600d, 0x6005 };
static const uint16_t* CSW[] = { nullptr, nullptr, nullptr, csw3, csw4, csw5, csw6 };

// Absolute from mag-sign
template<typename T> static T magsabs(T v) { return (v >> 1) + (v & 1); }

// integer divide count(in magsign) by cf(normal, positive)
template<typename T> static T magsdiv(T val, T cf) {return ((magsabs(val) / cf) << 1) - (val & 1);}

// greatest common factor (absolute) of a B2 sized vector of mag-sign values, Euclid algorithm
template<typename T> static T gcf(const T* group){
    static_assert(std::is_integral<T>() && std::is_unsigned<T>(), "Only unsigned integer types allowed");
    // Work with absolute values
    int sz = 0;
    T v[B2] = {}; // No need to initialize
    for (int i = 0; i < B2; i++) {
        v[sz] = magsabs(group[i]);
        sz += 0 < v[sz]; // Skip the zeros
    }
    do {
        // minimum value and location
        T m = v[0];
        int j = 0;
        for (int i = 1; i < sz; i++)
            if (m > v[i])
                m = v[j = i];
        if (1 == m)
            return 1; // common factor
        // Force the min at v[0]
        v[j] = v[0];
        v[0] = m;
        for (int i = j = 1; i < sz; i++) {
            v[j] = v[i] % m;
            j += 0 < v[j]; // Skip the zeros
        }
        sz = j; // Never zero
    } while (sz > 1);
    return v[0]; // common factor > 1
}

// Computed encoding with three codeword lenghts, used for higher rungs
// No conditionals, computes all three values and picks one by masking with the condition
// It is faster than similar code with conditions because the calculations for the three lines get interleaved
// The "(~0ull * (1 & <cond>))" is to show the compiler that it is a mask operation
static std::pair<size_t, uint64_t> qb3csz(uint64_t val, size_t rung) {
    assert(rung > 1); // Works for rungs 2+
    uint64_t nxt = (val >> (rung - 1)) & 1;
    uint64_t top = val >> rung;
    uint64_t tb = 1ull << rung;
    return std::make_pair<size_t, uint64_t>(rung + top + (top | nxt),
          ((~0ull * (1 & ~(top | nxt))) & (val << 1))                     // 0 0 SHORT    -> _x0
        + ((~0ull * (1 & (~top & nxt))) & ((((val << 1) ^ tb) << 1) | 1)) // 0 1 NOMINAL  -> _01
        + ((~0ull * (1 & top))          & (((val ^ tb) << 2) | 0b11)));   // 1 x LONG     -> _11
}

// Single value QB3 encode, possibly using tables, works for all rungs
static std::pair<size_t, uint64_t> qb3csztbl(uint64_t val, size_t rung) {
    if ((sizeof(CRG) / sizeof(*CRG)) > rung) {
        auto cs = CRG[rung][val];
        return std::make_pair<size_t, uint64_t>(cs >> 12, cs & TBLMASK);
    }
    return qb3csz(val, rung);
}

// only encode the group entries, not the rung switch
// maxval is used to choose the rung for encoding
// If abits > 0, the accumulator is also pushed into the stream
template <typename T>
static void groupencode(T group[B2], T maxval, oBits& s, uint64_t acc, size_t abits)
{
    assert(abits <= 64);
    const size_t rung = topbit(maxval | 1);
    if (0 == rung) { // only 1s and 0s, rung is -1 or 0
        acc |= static_cast<uint64_t>(maxval) << abits++;
        if (0 != maxval)
            for (int i = 0; i < B2; i++)
                acc |= static_cast<uint64_t>(group[i]) << abits++;
        s.push(acc, abits);
        return;
    }
    // Flip the last set rung bit if the rung bit sequence is a step down
    // At least one rung bit has to be set, so it can't return 0
    auto stepp = step(group, rung);
    assert(stepp > 0); // At least one rung bit should be set
    if (stepp <= B2)
        group[stepp - 1] ^= static_cast<T>(1ull << rung);
    if (abits > 8) { // Just in case, a rung switch is 8 bits at most
        s.push(acc, abits);
        acc = abits = 0;
    }
    if (6 > rung) { // Half of the group fits in 64 bits
        auto t = CRG[rung];
        for (size_t i = 0; i < B2 / 2; i++) {
            acc |= (TBLMASK & t[group[i]]) << abits;
            abits += t[group[i]] >> 12;
        }
        // No need to push accumulator at rung 1 and sometimes 2
        if (rung != 1 && (rung != 2 || abits > 32)) {
            s.push(acc, abits);
            acc = abits = 0;
        }

        for (size_t i = B2 / 2; i < B2; i++) {
            acc |= (TBLMASK & t[group[i]]) << abits;
            abits += t[group[i]] >> 12;
        }
        s.push(acc, abits);
    }
    // Last part of table encoding, rung 6-7
    // Encoded data fits in 256 bits, 4 way interleaved
    else if ((sizeof(CRG) / sizeof(*CRG)) > rung) {
        auto t = CRG[rung];
        uint64_t a[4] = { acc, 0, 0, 0 };
        size_t asz[4] = { abits, 0, 0, 0 };
        for (size_t i = 0; i < B; i++) {
            for (size_t j = 0; j < B; j++) {
                auto v = t[group[j * B + i]];
                a[j] |= (TBLMASK & v) << asz[j];
                asz[j] += v >> 12;
            }
        }
        for (size_t i = 0; i < B; i++)
            s.push(a[i], asz[i]);
    }
    // Computed encoding, slower, works for rung > 1
    else if (1 < sizeof(T)) { // This vanishes in 8 bit mode
        if (sizeof(T) < 8 || rung < 31) { // low rung, can reuse acc
            for (int i = 0; i < B2; i++) {
                auto p = qb3csz(group[i], rung);
                if (p.first + abits > 64) {
                    s.push(acc, abits);
                    acc = abits = 0;
                }
                acc |= p.second << abits;
                abits += p.first;
            }
            s.push(acc, abits);
        }
        else { // sizeof(T) == 8
            s.push(acc, abits);
            if (rung < 63) { // high rung, no overflow
                for (int i = 0; i < B2; i++)
                    s.push(qb3csz(group[i], rung));
            }
            else { // rung 63 might overflow 64 bits
                for (int i = 0; i < B2; i++) {
                    auto p = qb3csz(group[i], rung);
                    size_t ovf = p.first & (p.first >> 6); // overflow
                    s.push(p.second, p.first ^ ovf); // changes 65 in 64
                    if (ovf)
                        s.push(1ull & (static_cast<uint64_t>(group[i]) >> 62), ovf);
                }
            }
        }
    }
    if (stepp <= B2) // Leave the group as found
        group[stepp - 1] ^= static_cast<T>(1ull << rung);
}

// Base QB3 group encode with code switch, returns encoded size
template <typename T>
static void groupencode(T group[B2], T maxval, size_t oldrung, oBits& s) {
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    uint64_t acc = CSW[UBITS][(topbit(maxval | 1) - oldrung) & ((1ull << UBITS) - 1)];
    groupencode(group, maxval, s, acc & TBLMASK, static_cast<size_t>(acc >> 12));
}

// Group encode with cf
template <typename T>
static void cfgenc(const T igrp[B2], T cf, T pcf, size_t oldrung, oBits& bits) {
    // Signal as switch to same rung, max-positive value, by UBITS
    const uint16_t SIGNAL[] = { 0x0, 0x0, 0x0, 0x5017, 0x6037, 0x7077, 0x80f7 };
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    auto csw = CSW[UBITS];
    // Start with the CF encoding signal
    uint64_t acc = SIGNAL[UBITS] & TBLMASK;
    size_t abits = UBITS + 2; // SIGNAL >> 12
    // divide raw absolute group values by CF and find the new maxvalue
    T maxval = 0;
    T group[B2] = {};
    for (size_t i = 0; i < B2; i++) if (igrp[i])
        maxval = std::max(maxval, group[i] = magsdiv(igrp[i], cf));
            //T(((magsabs(igrp[i]) / cf) << 1) - (igrp[i] & 1)));
    cf -= 2; // Bias down, 0 and 1 are not used
    auto trung = topbit(maxval | 1); // rung for the group values
    auto cfrung = topbit(cf | 1);    // rung for cf-2 value
    auto cs = csw[(trung - oldrung) & ((1ull << UBITS) - 1)];
    if ((cs >> 12) == 1) // no-switch, use signal instead, it decodes to delta of zero
        cs = SIGNAL[UBITS];
    // Always encode new rung, without the change flag
    acc |= (cs & (TBLMASK - 1)) << (abits - 1);
    abits += static_cast<size_t>((cs >> 12) - 1);
    // Then encode the diff-cf flag
    if (cf != pcf) {
        acc |= 1ull << abits++;
        if (trung >= cfrung && (trung < (cfrung + UBITS) || 0 == cfrung)) {
            // cf encoded at trung
            abits++;
            if (0 == trung) { // Single bit encoding, maxval is 1, no need for the all-zero flag
                acc |= static_cast<uint64_t>(cf) << abits++; // Last bit of cf
                for (int i = 0; i < B2; i++)
                    acc |= static_cast<uint64_t>(group[i]) << abits++;
                bits.push(acc, abits);
                return;
            }
            auto p = qb3csztbl(cf, trung);
            bits.push(acc, abits); // could overflow, safter this way
            acc = p.second;
            abits = p.first;
        }
        else {
            // cf encoded with own rung, encoded as difference from trung
            cs = csw[(cfrung - trung) & ((1ull << UBITS) - 1)]; // Never 0
            acc |= (cs & TBLMASK) << abits;
            abits += cs >> 12;
            // Followed by cf itself, encoded one rung under where it should be
            auto p = qb3csztbl(cf ^ (1ull << cfrung), cfrung - 1);
            if (p.first + abits > 64) {
                bits.push(acc, abits);
                acc = abits = 0;
            }
            acc |= p.second << abits;
            abits += p.first;
            // For trung == 0, save the flag bit, can't be all zeros
            if (0 == trung) {
                if (abits + B2 > 64) { // Large CF
                    bits.push(acc, abits);
                    acc = abits = 0;
                }
                for (int i = 0; i < B2; i++)
                    acc |= static_cast<uint64_t>(group[i]) << abits++;
                bits.push(acc, abits);
                return;
            }
        }
    }
    else { // Same cf, encoded as a single zero bit
        abits++;
        if (0 == trung) { // Single bit group encoding, no need for the all-zero flag
            for (int i = 0; i < B2; i++)
                acc |= static_cast<uint64_t>(group[i]) << abits++;
            bits.push(acc, abits);
            return;
        }
    }
    // Encode the group only, divided by CF
    groupencode(group, maxval, bits, acc, abits);
}

// Check that the parameters are valid
static int check_info(const encs& info) {
    if (info.xsize < 4 || info.xsize > 0x10000 || info.ysize < 4 || info.ysize > 0x10000
        || info.nbands < 1 || info.nbands > QB3_MAXBANDS)
        return 1;
    // Check band mapping
    for (size_t c = 0; c < info.nbands; c++)
        if (info.cband[c] >= info.nbands)
            return 2; // Band mapping error
    return 0;
}

// Only basic encoding
template<typename T>
static int encode_fast(const T* image, oBits& s, encs &info)
{
    static_assert(std::is_integral<T>() && std::is_unsigned<T>(), "Only unsigned integer types allowed");
    if (check_info(info))
        return check_info(info);
    const size_t xsize(info.xsize), ysize(info.ysize), bands(info.nbands), *cband(info.cband);
    // Running code length, start with nominal value
    size_t runbits[QB3_MAXBANDS] = {};
    // Previous value, per band
    T prev[QB3_MAXBANDS] = {};
    // Initialize stage
    for (size_t c = 0; c < bands; c++) {
        runbits[c] = info.band[c].runbits;
        prev[c] = static_cast<T>(info.band[c].prev);
    }
    // Set up block offsets based on traversal order, defaults to HILBERT
    // Best block traversal order
    const uint64_t order(info.order ? info.order : HILBERT);
    size_t offset[B2] = {};
    for (size_t i = 0; i < B2; i++) {
        // Pick up one nibbles, in top to bottom order
        size_t n = (order >> ((B2 - 1 - i) << 2));
        offset[i] = (xsize * ((n >> 2) & 0b11) + (n & 0b11)) * bands;
    }
    T group[B2] = {};
    for (size_t y = 0; y < ysize; y += B) {
        // If the last row is partial, roll it up
        if (y + B > ysize)
            y = ysize - B;
        for (size_t x = 0; x < xsize; x += B) {
            // If the last column is partial, move it left
            if (x + B > xsize)
                x = xsize - B;                
            const size_t loc = (y * xsize + x) * bands; // Top-left pixel address
            for (size_t c = 0; c < bands; c++) { // blocks are band interleaved
                T maxval(0); // Maximum mag-sign value within this group
                // Collect the block for this band, convert to running delta mag-sign
                auto prv = prev[c];
                // Use separate loop for basebands to avoid a test inside the hot loop
                if (c != cband[c]) {
                    auto cb = cband[c];
                    for (size_t i = 0; i < B2; i++) {
                        T g = image[loc + c + offset[i]] - image[loc + cb + offset[i]];
                        prv += g -= prv;
                        group[i] = g = mags(g);
                        if (maxval < g) maxval = g;
                    }
                }
                else { // baseband
                    for (size_t i = 0; i < B2; i++) {
                        T g = image[loc + c + offset[i]];
                        prv += g -= prv;
                        group[i] = g = mags(g);
                        if (maxval < g) maxval = g;
                    }
                }
                prev[c] = prv;
                groupencode(group, maxval, runbits[c], s);
                runbits[c] = topbit(maxval | 1);
            }
        }
    }
    // Save the state
    for (size_t c = 0; c < bands; c++) {
        info.band[c].prev = static_cast<size_t>(prev[c]);
        info.band[c].runbits = runbits[c];
    }
    return 0;
}

// Index based encoding
template<typename T>
static int ienc(const T grp[B2], size_t rung, size_t oldrung, oBits &s) {
    constexpr int TOO_LARGE(800); // Larger than any possible size
    if (rung < 4 || rung == 63) // TODO: encode rung 63
        return TOO_LARGE;
    struct KVP { T key, count; };
    KVP v[B2 / 2] = {};
    size_t len = 0;
    for (int i = 0; i < B2; i++) {
        size_t j = 0;
        while (j < len && v[j].key != grp[i])
            if (++j >= B2 /2)
                return TOO_LARGE; // Too many unique values
        if (j == len)
            v[len++] = { grp[i], 1 };
        else
            v[j].count++;
    }

    const uint16_t SIGNAL[] = { 0x0, 0x0, 0x0, 0x5017, 0x6037, 0x7077, 0x80f7 };
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    constexpr auto NORM_MASK((1ull << UBITS) - 1); // UBITS set
    auto csw = CSW[UBITS];

    // Start with the encoding signal
    uint64_t acc = SIGNAL[UBITS] & TBLMASK;
    size_t abits = UBITS + 2; // SIGNAL >> 12

    // The switch from oldrung to max rung, signal for index encoding
    auto cs = csw[(NORM_MASK - oldrung) & NORM_MASK];
    if ((cs >> 12) == 1) // no-switch, use signal instead, it decodes to delta of zero
        cs = SIGNAL[UBITS];
    acc |= (cs & (TBLMASK - 1)) << (abits - 1);
    abits += static_cast<size_t>((cs >> 12) - 1);

    // Encode the real rung
    cs = csw[(rung - oldrung) & NORM_MASK];
    if ((cs >> 12) == 1) // no-switch, use signal instead, it decodes to delta of zero
        cs = SIGNAL[UBITS];
    acc |= (cs & (TBLMASK - 1)) << (abits - 1);
    abits += static_cast<size_t>((cs >> 12) - 1);
    s.push(acc, abits);
    acc = abits = 0;
    std::sort(v, v + len, [](const KVP& a, const KVP& b) { return a.count > b.count; });

    // Encode indices
    for (int i = 0; i < B2; i++) {
        int j = 0;
        while (v[j].key != grp[i])
            j++;
        auto c = crg2[j];
        acc |= (c & TBLMASK) << abits;
        abits += c >> 12;
    }
    s.push(acc, abits);
    // Encode unique values in order of frequency
    for (int i = 0; i < len; i++)
        s.push(qb3csztbl(v[i].key, rung));
    return static_cast<int>(s.position());
}

// Returns error code or 0 if success
// TODO: Error code mapping
template <typename T = uint8_t>
static int encode_best(const T *image, oBits& s, encs &info)
{
    static_assert(std::is_integral<T>() && std::is_unsigned<T>(), "Only unsigned integer types allowed");
    if (check_info(info))
        return check_info(info);
    const size_t xsize(info.xsize), ysize(info.ysize), bands(info.nbands), *cband(info.cband);
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    auto csw = CSW[UBITS];
    // Running code length, start with nominal value
    size_t runbits[QB3_MAXBANDS] = {};
    // Previous values, per band
    T prev[QB3_MAXBANDS] = {}, pcf[QB3_MAXBANDS] = {};
    for (size_t c = 0; c < bands; c++) {
        runbits[c] = info.band[c].runbits;
        prev[c] = static_cast<T>(info.band[c].prev);
        pcf[c] = static_cast<T>(info.band[c].cf);
    }
    // Set up block offsets based on traversal order, defaults to HILBERT
    uint64_t order(info.order ? info.order : HILBERT);
    size_t offset[B2] = {};
    for (size_t i = 0; i < B2; i++) {
        // Pick up one nibble, in top to bottom order
        size_t n = (order >> ((B2 - 1 - i) << 2));
        offset[i] = (xsize * ((n >> 2) & 0b11) + (n & 0b11)) * bands;
    }
    T group[B2] = {}; // 2D group to encode
    for (size_t y = 0; y < ysize; y += B) {
        // If the last row is partial, roll it up
        if (y + B > ysize)
            y = ysize - B;
        for (size_t x = 0; x < xsize; x += B) {
            // If the last column is partial, move it left
            if (x + B > xsize)
                x = xsize - B;
            size_t loc = (y * xsize + x) * bands; // Top-left pixel address
            for (size_t c = 0; c < bands; c++) { // blocks are always band interleaved
                T maxval(0); // Maximum mag-sign value within this group
                // Collect the block for this band, convert to running delta mag-sign
                auto prv = prev[c];
                if (c != cband[c]) {
                    auto cb = cband[c];
                    for (size_t i = 0; i < B2; i++) {
                        T g = image[loc + c + offset[i]] - image[loc + cb + offset[i]];
                        prv += g -= prv;
                        group[i] = g = mags(g);
                        if (maxval < g) maxval = g;
                    }
                }
                else {
                    for (size_t i = 0; i < B2; i++) {
                        T g = image[loc + c + offset[i]];
                        prv += g -= prv;
                        group[i] = g = mags(g);
                        if (maxval < g) maxval = g;
                    }
                }
                prev[c] = prv;
                auto oldrung = runbits[c];
                const size_t rung = topbit(maxval | 1);
                runbits[c] = rung;
                if (0 == rung) { // only 1s and 0s, rung is -1 or 0, no cf
                    uint64_t acc = csw[(rung - oldrung) & ((1ull << UBITS) - 1)];
                    size_t abits = acc >> 12;
                    acc &= TBLMASK;
                    acc |= static_cast<uint64_t>(maxval) << abits++; // Add the all-zero flag
                    if (0 != maxval)
                        for (size_t i = 0; i < B2; i++)
                            acc |= static_cast<uint64_t>(group[i]) << abits++;
                    s.push(acc, abits);
                    continue;
                }

                auto cf = gcf(group);
                auto start = s.position();
                if (cf < 2)
                    groupencode(group, maxval, oldrung, s);
                else
                    cfgenc(group, cf, pcf[c], oldrung, s);

                if ((s.position() - start) >= (36 + 3 * UBITS + 2 * rung)) {
                    uint8_t buffer[128] = {};
                    oBits idxs(buffer);
                    int idx = ienc(group, rung, oldrung, idxs);
                    if (idx < s.position() - start) {
                        s.rewind(start);
                        s += idxs;
                    } else if (cf > 1 && pcf[c] != cf - 2)
                        pcf[c] = cf - 2;
                }
                else if (cf > 1 && pcf[c] != cf - 2)
                    pcf[c] = cf - 2;
            }
        }
    }
    // Save the state
    for (size_t c = 0; c < bands; c++) {
        info.band[c].prev = static_cast<size_t>(prev[c]);
        info.band[c].runbits = runbits[c];
        info.band[c].cf = static_cast<size_t>(pcf[c]);
    }
    return 0;
}

// Decode 
// Decoding tables, twice as large as the encoding ones
// 2k for 0-7
static const uint16_t drg0[] = { 0x1000, 0x1001, 0x1000, 0x1001 };
static const uint16_t drg1[] = { 0x1000, 0x2001, 0x1000, 0x3002, 0x1000, 0x2001, 0x1000, 0x3003 };
static const uint16_t drg2[] = { 0x2000, 0x3002, 0x2001, 0x4004, 0x2000, 0x3003, 0x2001, 0x4005, 0x2000, 0x3002, 0x2001, 0x4006,
0x2000, 0x3003, 0x2001, 0x4007 };
static const uint16_t drg3[] = { 0x3000, 0x4004, 0x3001, 0x5008, 0x3002, 0x4005, 0x3003, 0x5009, 0x3000, 0x4006, 0x3001, 0x500a,
0x3002, 0x4007, 0x3003, 0x500b, 0x3000, 0x4004, 0x3001, 0x500c, 0x3002, 0x4005, 0x3003, 0x500d, 0x3000, 0x4006, 0x3001, 0x500e,
0x3002, 0x4007, 0x3003, 0x500f };
static const uint16_t drg4[] = { 0x4000, 0x5008, 0x4001, 0x6010, 0x4002, 0x5009, 0x4003, 0x6011, 0x4004, 0x500a, 0x4005, 0x6012,
0x4006, 0x500b, 0x4007, 0x6013, 0x4000, 0x500c, 0x4001, 0x6014, 0x4002, 0x500d, 0x4003, 0x6015, 0x4004, 0x500e, 0x4005, 0x6016,
0x4006, 0x500f, 0x4007, 0x6017, 0x4000, 0x5008, 0x4001, 0x6018, 0x4002, 0x5009, 0x4003, 0x6019, 0x4004, 0x500a, 0x4005, 0x601a,
0x4006, 0x500b, 0x4007, 0x601b, 0x4000, 0x500c, 0x4001, 0x601c, 0x4002, 0x500d, 0x4003, 0x601d, 0x4004, 0x500e, 0x4005, 0x601e,
0x4006, 0x500f, 0x4007, 0x601f };
static const uint16_t drg5[] = { 0x5000, 0x6010, 0x5001, 0x7020, 0x5002, 0x6011, 0x5003, 0x7021, 0x5004, 0x6012, 0x5005, 0x7022,
0x5006, 0x6013, 0x5007, 0x7023, 0x5008, 0x6014, 0x5009, 0x7024, 0x500a, 0x6015, 0x500b, 0x7025, 0x500c, 0x6016, 0x500d, 0x7026,
0x500e, 0x6017, 0x500f, 0x7027, 0x5000, 0x6018, 0x5001, 0x7028, 0x5002, 0x6019, 0x5003, 0x7029, 0x5004, 0x601a, 0x5005, 0x702a,
0x5006, 0x601b, 0x5007, 0x702b, 0x5008, 0x601c, 0x5009, 0x702c, 0x500a, 0x601d, 0x500b, 0x702d, 0x500c, 0x601e, 0x500d, 0x702e,
0x500e, 0x601f, 0x500f, 0x702f, 0x5000, 0x6010, 0x5001, 0x7030, 0x5002, 0x6011, 0x5003, 0x7031, 0x5004, 0x6012, 0x5005, 0x7032,
0x5006, 0x6013, 0x5007, 0x7033, 0x5008, 0x6014, 0x5009, 0x7034, 0x500a, 0x6015, 0x500b, 0x7035, 0x500c, 0x6016, 0x500d, 0x7036,
0x500e, 0x6017, 0x500f, 0x7037, 0x5000, 0x6018, 0x5001, 0x7038, 0x5002, 0x6019, 0x5003, 0x7039, 0x5004, 0x601a, 0x5005, 0x703a,
0x5006, 0x601b, 0x5007, 0x703b, 0x5008, 0x601c, 0x5009, 0x703c, 0x500a, 0x601d, 0x500b, 0x703d, 0x500c, 0x601e, 0x500d, 0x703e,
0x500e, 0x601f, 0x500f, 0x703f };
static const uint16_t drg6[] = { 0x6000, 0x7020, 0x6001, 0x8040, 0x6002, 0x7021, 0x6003, 0x8041, 0x6004, 0x7022, 0x6005, 0x8042,
0x6006, 0x7023, 0x6007, 0x8043, 0x6008, 0x7024, 0x6009, 0x8044, 0x600a, 0x7025, 0x600b, 0x8045, 0x600c, 0x7026, 0x600d, 0x8046,
0x600e, 0x7027, 0x600f, 0x8047, 0x6010, 0x7028, 0x6011, 0x8048, 0x6012, 0x7029, 0x6013, 0x8049, 0x6014, 0x702a, 0x6015, 0x804a,
0x6016, 0x702b, 0x6017, 0x804b, 0x6018, 0x702c, 0x6019, 0x804c, 0x601a, 0x702d, 0x601b, 0x804d, 0x601c, 0x702e, 0x601d, 0x804e,
0x601e, 0x702f, 0x601f, 0x804f, 0x6000, 0x7030, 0x6001, 0x8050, 0x6002, 0x7031, 0x6003, 0x8051, 0x6004, 0x7032, 0x6005, 0x8052,
0x6006, 0x7033, 0x6007, 0x8053, 0x6008, 0x7034, 0x6009, 0x8054, 0x600a, 0x7035, 0x600b, 0x8055, 0x600c, 0x7036, 0x600d, 0x8056,
0x600e, 0x7037, 0x600f, 0x8057, 0x6010, 0x7038, 0x6011, 0x8058, 0x6012, 0x7039, 0x6013, 0x8059, 0x6014, 0x703a, 0x6015, 0x805a,
0x6016, 0x703b, 0x6017, 0x805b, 0x6018, 0x703c, 0x6019, 0x805c, 0x601a, 0x703d, 0x601b, 0x805d, 0x601c, 0x703e, 0x601d, 0x805e,
0x601e, 0x703f, 0x601f, 0x805f, 0x6000, 0x7020, 0x6001, 0x8060, 0x6002, 0x7021, 0x6003, 0x8061, 0x6004, 0x7022, 0x6005, 0x8062,
0x6006, 0x7023, 0x6007, 0x8063, 0x6008, 0x7024, 0x6009, 0x8064, 0x600a, 0x7025, 0x600b, 0x8065, 0x600c, 0x7026, 0x600d, 0x8066,
0x600e, 0x7027, 0x600f, 0x8067, 0x6010, 0x7028, 0x6011, 0x8068, 0x6012, 0x7029, 0x6013, 0x8069, 0x6014, 0x702a, 0x6015, 0x806a,
0x6016, 0x702b, 0x6017, 0x806b, 0x6018, 0x702c, 0x6019, 0x806c, 0x601a, 0x702d, 0x601b, 0x806d, 0x601c, 0x702e, 0x601d, 0x806e,
0x601e, 0x702f, 0x601f, 0x806f, 0x6000, 0x7030, 0x6001, 0x8070, 0x6002, 0x7031, 0x6003, 0x8071, 0x6004, 0x7032, 0x6005, 0x8072,
0x6006, 0x7033, 0x6007, 0x8073, 0x6008, 0x7034, 0x6009, 0x8074, 0x600a, 0x7035, 0x600b, 0x8075, 0x600c, 0x7036, 0x600d, 0x8076,
0x600e, 0x7037, 0x600f, 0x8077, 0x6010, 0x7038, 0x6011, 0x8078, 0x6012, 0x7039, 0x6013, 0x8079, 0x6014, 0x703a, 0x6015, 0x807a,
0x6016, 0x703b, 0x6017, 0x807b, 0x6018, 0x703c, 0x6019, 0x807c, 0x601a, 0x703d, 0x601b, 0x807d, 0x601c, 0x703e, 0x601d, 0x807e,
0x601e, 0x703f, 0x601f, 0x807f };
static const uint16_t drg7[] = { 0x7000, 0x8040, 0x7001, 0x9080, 0x7002, 0x8041, 0x7003, 0x9081, 0x7004, 0x8042, 0x7005, 0x9082,
0x7006, 0x8043, 0x7007, 0x9083, 0x7008, 0x8044, 0x7009, 0x9084, 0x700a, 0x8045, 0x700b, 0x9085, 0x700c, 0x8046, 0x700d, 0x9086,
0x700e, 0x8047, 0x700f, 0x9087, 0x7010, 0x8048, 0x7011, 0x9088, 0x7012, 0x8049, 0x7013, 0x9089, 0x7014, 0x804a, 0x7015, 0x908a,
0x7016, 0x804b, 0x7017, 0x908b, 0x7018, 0x804c, 0x7019, 0x908c, 0x701a, 0x804d, 0x701b, 0x908d, 0x701c, 0x804e, 0x701d, 0x908e,
0x701e, 0x804f, 0x701f, 0x908f, 0x7020, 0x8050, 0x7021, 0x9090, 0x7022, 0x8051, 0x7023, 0x9091, 0x7024, 0x8052, 0x7025, 0x9092,
0x7026, 0x8053, 0x7027, 0x9093, 0x7028, 0x8054, 0x7029, 0x9094, 0x702a, 0x8055, 0x702b, 0x9095, 0x702c, 0x8056, 0x702d, 0x9096,
0x702e, 0x8057, 0x702f, 0x9097, 0x7030, 0x8058, 0x7031, 0x9098, 0x7032, 0x8059, 0x7033, 0x9099, 0x7034, 0x805a, 0x7035, 0x909a,
0x7036, 0x805b, 0x7037, 0x909b, 0x7038, 0x805c, 0x7039, 0x909c, 0x703a, 0x805d, 0x703b, 0x909d, 0x703c, 0x805e, 0x703d, 0x909e,
0x703e, 0x805f, 0x703f, 0x909f, 0x7000, 0x8060, 0x7001, 0x90a0, 0x7002, 0x8061, 0x7003, 0x90a1, 0x7004, 0x8062, 0x7005, 0x90a2,
0x7006, 0x8063, 0x7007, 0x90a3, 0x7008, 0x8064, 0x7009, 0x90a4, 0x700a, 0x8065, 0x700b, 0x90a5, 0x700c, 0x8066, 0x700d, 0x90a6,
0x700e, 0x8067, 0x700f, 0x90a7, 0x7010, 0x8068, 0x7011, 0x90a8, 0x7012, 0x8069, 0x7013, 0x90a9, 0x7014, 0x806a, 0x7015, 0x90aa,
0x7016, 0x806b, 0x7017, 0x90ab, 0x7018, 0x806c, 0x7019, 0x90ac, 0x701a, 0x806d, 0x701b, 0x90ad, 0x701c, 0x806e, 0x701d, 0x90ae,
0x701e, 0x806f, 0x701f, 0x90af, 0x7020, 0x8070, 0x7021, 0x90b0, 0x7022, 0x8071, 0x7023, 0x90b1, 0x7024, 0x8072, 0x7025, 0x90b2,
0x7026, 0x8073, 0x7027, 0x90b3, 0x7028, 0x8074, 0x7029, 0x90b4, 0x702a, 0x8075, 0x702b, 0x90b5, 0x702c, 0x8076, 0x702d, 0x90b6,
0x702e, 0x8077, 0x702f, 0x90b7, 0x7030, 0x8078, 0x7031, 0x90b8, 0x7032, 0x8079, 0x7033, 0x90b9, 0x7034, 0x807a, 0x7035, 0x90ba,
0x7036, 0x807b, 0x7037, 0x90bb, 0x7038, 0x807c, 0x7039, 0x90bc, 0x703a, 0x807d, 0x703b, 0x90bd, 0x703c, 0x807e, 0x703d, 0x90be,
0x703e, 0x807f, 0x703f, 0x90bf, 0x7000, 0x8040, 0x7001, 0x90c0, 0x7002, 0x8041, 0x7003, 0x90c1, 0x7004, 0x8042, 0x7005, 0x90c2,
0x7006, 0x8043, 0x7007, 0x90c3, 0x7008, 0x8044, 0x7009, 0x90c4, 0x700a, 0x8045, 0x700b, 0x90c5, 0x700c, 0x8046, 0x700d, 0x90c6,
0x700e, 0x8047, 0x700f, 0x90c7, 0x7010, 0x8048, 0x7011, 0x90c8, 0x7012, 0x8049, 0x7013, 0x90c9, 0x7014, 0x804a, 0x7015, 0x90ca,
0x7016, 0x804b, 0x7017, 0x90cb, 0x7018, 0x804c, 0x7019, 0x90cc, 0x701a, 0x804d, 0x701b, 0x90cd, 0x701c, 0x804e, 0x701d, 0x90ce,
0x701e, 0x804f, 0x701f, 0x90cf, 0x7020, 0x8050, 0x7021, 0x90d0, 0x7022, 0x8051, 0x7023, 0x90d1, 0x7024, 0x8052, 0x7025, 0x90d2,
0x7026, 0x8053, 0x7027, 0x90d3, 0x7028, 0x8054, 0x7029, 0x90d4, 0x702a, 0x8055, 0x702b, 0x90d5, 0x702c, 0x8056, 0x702d, 0x90d6,
0x702e, 0x8057, 0x702f, 0x90d7, 0x7030, 0x8058, 0x7031, 0x90d8, 0x7032, 0x8059, 0x7033, 0x90d9, 0x7034, 0x805a, 0x7035, 0x90da,
0x7036, 0x805b, 0x7037, 0x90db, 0x7038, 0x805c, 0x7039, 0x90dc, 0x703a, 0x805d, 0x703b, 0x90dd, 0x703c, 0x805e, 0x703d, 0x90de,
0x703e, 0x805f, 0x703f, 0x90df, 0x7000, 0x8060, 0x7001, 0x90e0, 0x7002, 0x8061, 0x7003, 0x90e1, 0x7004, 0x8062, 0x7005, 0x90e2,
0x7006, 0x8063, 0x7007, 0x90e3, 0x7008, 0x8064, 0x7009, 0x90e4, 0x700a, 0x8065, 0x700b, 0x90e5, 0x700c, 0x8066, 0x700d, 0x90e6,
0x700e, 0x8067, 0x700f, 0x90e7, 0x7010, 0x8068, 0x7011, 0x90e8, 0x7012, 0x8069, 0x7013, 0x90e9, 0x7014, 0x806a, 0x7015, 0x90ea,
0x7016, 0x806b, 0x7017, 0x90eb, 0x7018, 0x806c, 0x7019, 0x90ec, 0x701a, 0x806d, 0x701b, 0x90ed, 0x701c, 0x806e, 0x701d, 0x90ee,
0x701e, 0x806f, 0x701f, 0x90ef, 0x7020, 0x8070, 0x7021, 0x90f0, 0x7022, 0x8071, 0x7023, 0x90f1, 0x7024, 0x8072, 0x7025, 0x90f2,
0x7026, 0x8073, 0x7027, 0x90f3, 0x7028, 0x8074, 0x7029, 0x90f4, 0x702a, 0x8075, 0x702b, 0x90f5, 0x702c, 0x8076, 0x702d, 0x90f6,
0x702e, 0x8077, 0x702f, 0x90f7, 0x7030, 0x8078, 0x7031, 0x90f8, 0x7032, 0x8079, 0x7033, 0x90f9, 0x7034, 0x807a, 0x7035, 0x90fa,
0x7036, 0x807b, 0x7037, 0x90fb, 0x7038, 0x807c, 0x7039, 0x90fc, 0x703a, 0x807d, 0x703b, 0x90fd, 0x703c, 0x807e, 0x703d, 0x90fe,
0x703e, 0x807f, 0x703f, 0x90ff };

static const uint16_t* DRG[] = { drg0, drg1, drg2, drg3, drg4, drg5, drg6, drg7 };

// rung 1 and 2 double value decoding tables, can use 8 bits
static const uint8_t DDRG1[] = { 0x20, 0x31, 0x34, 0x42, 0x20, 0x45, 0x48, 0x43, 0x20, 0x31, 0x34, 0x56, 0x20, 0x59, 0x4c,
0x57, 0x20, 0x31, 0x34, 0x42, 0x20, 0x45, 0x48, 0x43, 0x20, 0x31, 0x34, 0x6a, 0x20, 0x5d, 0x4c, 0x6b, 0x20, 0x31, 0x34, 0x42,
0x20, 0x45, 0x48, 0x43, 0x20, 0x31, 0x34, 0x56, 0x20, 0x59, 0x4c, 0x57, 0x20, 0x31, 0x34, 0x42, 0x20, 0x45, 0x48, 0x43, 0x20,
0x31, 0x34, 0x6e, 0x20, 0x5d, 0x4c, 0x6f };
static const uint16_t DDRG2[] = { 0x4000, 0x5002, 0x4001, 0x6004, 0x5010, 0x5003, 0x5011, 0x6005, 0x4008, 0x6012, 0x4009,
0x6006, 0x6020, 0x6013, 0x6021, 0x6007, 0x4000, 0x500a, 0x4001, 0x7014, 0x5018, 0x500b, 0x5019, 0x7015, 0x4008, 0x7022, 0x4009,
0x7016, 0x6028, 0x7023, 0x6029, 0x7017, 0x4000, 0x5002, 0x4001, 0x600c, 0x5010, 0x5003, 0x5011, 0x600d, 0x4008, 0x601a, 0x4009,
0x600e, 0x6030, 0x601b, 0x6031, 0x600f, 0x4000, 0x500a, 0x4001, 0x8024, 0x5018, 0x500b, 0x5019, 0x8025, 0x4008, 0x702a, 0x4009,
0x8026, 0x6038, 0x702b, 0x6039, 0x8027, 0x4000, 0x5002, 0x4001, 0x6004, 0x5010, 0x5003, 0x5011, 0x6005, 0x4008, 0x6012, 0x4009,
0x6006, 0x6020, 0x6013, 0x6021, 0x6007, 0x4000, 0x500a, 0x4001, 0x701c, 0x5018, 0x500b, 0x5019, 0x701d, 0x4008, 0x7032, 0x4009,
0x701e, 0x6028, 0x7033, 0x6029, 0x701f, 0x4000, 0x5002, 0x4001, 0x600c, 0x5010, 0x5003, 0x5011, 0x600d, 0x4008, 0x601a, 0x4009,
0x600e, 0x6030, 0x601b, 0x6031, 0x600f, 0x4000, 0x500a, 0x4001, 0x802c, 0x5018, 0x500b, 0x5019, 0x802d, 0x4008, 0x703a, 0x4009,
0x802e, 0x6038, 0x703b, 0x6039, 0x802f, 0x4000, 0x5002, 0x4001, 0x6004, 0x5010, 0x5003, 0x5011, 0x6005, 0x4008, 0x6012, 0x4009,
0x6006, 0x6020, 0x6013, 0x6021, 0x6007, 0x4000, 0x500a, 0x4001, 0x7014, 0x5018, 0x500b, 0x5019, 0x7015, 0x4008, 0x7022, 0x4009,
0x7016, 0x6028, 0x7023, 0x6029, 0x7017, 0x4000, 0x5002, 0x4001, 0x600c, 0x5010, 0x5003, 0x5011, 0x600d, 0x4008, 0x601a, 0x4009,
0x600e, 0x6030, 0x601b, 0x6031, 0x600f, 0x4000, 0x500a, 0x4001, 0x8034, 0x5018, 0x500b, 0x5019, 0x8035, 0x4008, 0x702a, 0x4009,
0x8036, 0x6038, 0x702b, 0x6039, 0x8037, 0x4000, 0x5002, 0x4001, 0x6004, 0x5010, 0x5003, 0x5011, 0x6005, 0x4008, 0x6012, 0x4009,
0x6006, 0x6020, 0x6013, 0x6021, 0x6007, 0x4000, 0x500a, 0x4001, 0x701c, 0x5018, 0x500b, 0x5019, 0x701d, 0x4008, 0x7032, 0x4009,
0x701e, 0x6028, 0x7033, 0x6029, 0x701f, 0x4000, 0x5002, 0x4001, 0x600c, 0x5010, 0x5003, 0x5011, 0x600d, 0x4008, 0x601a, 0x4009,
0x600e, 0x6030, 0x601b, 0x6031, 0x600f, 0x4000, 0x500a, 0x4001, 0x803c, 0x5018, 0x500b, 0x5019, 0x803d, 0x4008, 0x703a, 0x4009,
0x803e, 0x6038, 0x703b, 0x6039, 0x803f };
// rung 3 double value would be 2k by itself, the normal one is 64 bytes, and it gets worse from there

// Decoding tables for codeswitch
static const uint16_t dsw3[] = { 0x3001, 0x4002, 0x3007, 0x5003, 0x3001, 0x4006, 0x3007, 0x5005, 0x3001, 0x4002, 0x3007, 0x5000,
0x3001, 0x4006, 0x3007, 0x5004 };
static const uint16_t dsw4[] = { 0x4001, 0x5003, 0x400f, 0x6005, 0x4002, 0x500d, 0x400e, 0x600b, 0x4001, 0x5004, 0x400f, 0x6006,
0x4002, 0x500c, 0x400e, 0x600a, 0x4001, 0x5003, 0x400f, 0x6007, 0x4002, 0x500d, 0x400e, 0x6009, 0x4001, 0x5004, 0x400f, 0x6000,
0x4002, 0x500c, 0x400e, 0x6008 };
static const uint16_t dsw5[] = { 0x5001, 0x6005, 0x501f, 0x7009, 0x5002, 0x601b, 0x501e, 0x7017, 0x5003, 0x6006, 0x501d, 0x700a,
0x5004, 0x601a, 0x501c, 0x7016, 0x5001, 0x6007, 0x501f, 0x700b, 0x5002, 0x6019, 0x501e, 0x7015, 0x5003, 0x6008, 0x501d, 0x700c,
0x5004, 0x6018, 0x501c, 0x7014, 0x5001, 0x6005, 0x501f, 0x700d, 0x5002, 0x601b, 0x501e, 0x7013, 0x5003, 0x6006, 0x501d, 0x700e,
0x5004, 0x601a, 0x501c, 0x7012, 0x5001, 0x6007, 0x501f, 0x700f, 0x5002, 0x6019, 0x501e, 0x7011, 0x5003, 0x6008, 0x501d, 0x7000,
0x5004, 0x6018, 0x501c, 0x7010 };
static const uint16_t dsw6[] = { 0x6001, 0x7009, 0x603f, 0x8011, 0x6002, 0x7037, 0x603e, 0x802f, 0x6003, 0x700a, 0x603d, 0x8012,
0x6004, 0x7036, 0x603c, 0x802e, 0x6005, 0x700b, 0x603b, 0x8013, 0x6006, 0x7035, 0x603a, 0x802d, 0x6007, 0x700c, 0x6039, 0x8014,
0x6008, 0x7034, 0x6038, 0x802c, 0x6001, 0x700d, 0x603f, 0x8015, 0x6002, 0x7033, 0x603e, 0x802b, 0x6003, 0x700e, 0x603d, 0x8016,
0x6004, 0x7032, 0x603c, 0x802a, 0x6005, 0x700f, 0x603b, 0x8017, 0x6006, 0x7031, 0x603a, 0x8029, 0x6007, 0x7010, 0x6039, 0x8018,
0x6008, 0x7030, 0x6038, 0x8028, 0x6001, 0x7009, 0x603f, 0x8019, 0x6002, 0x7037, 0x603e, 0x8027, 0x6003, 0x700a, 0x603d, 0x801a,
0x6004, 0x7036, 0x603c, 0x8026, 0x6005, 0x700b, 0x603b, 0x801b, 0x6006, 0x7035, 0x603a, 0x8025, 0x6007, 0x700c, 0x6039, 0x801c,
0x6008, 0x7034, 0x6038, 0x8024, 0x6001, 0x700d, 0x603f, 0x801d, 0x6002, 0x7033, 0x603e, 0x8023, 0x6003, 0x700e, 0x603d, 0x801e,
0x6004, 0x7032, 0x603c, 0x8022, 0x6005, 0x700f, 0x603b, 0x801f, 0x6006, 0x7031, 0x603a, 0x8021, 0x6007, 0x7010, 0x6039, 0x8000,
0x6008, 0x7030, 0x6038, 0x8020 };

// integer mag-sign to normal encoding without conditionals
template<typename T> static T smag(T v) { return (v >> 1) ^ (~T(0) * (v & 1)); }

// Computed decode, does not work for rung 0 or 1
static std::pair<size_t, uint64_t> qb3dsz(uint64_t val, size_t rung) {
    assert(rung > 1);
    uint64_t rbit = 1ull << rung;
    if (0 == (val & 1)) // Short
        return std::make_pair(rung, (val & (rbit - 1)) >> 1);
    uint64_t n = (val >> 1) & 1; // Next bit, long if set
    val = (val >> 2) & (rbit - 1);
    return std::make_pair(rung + 1 + n,
        (((1 & ~n) * ~0ull)  & (val | (rbit >> 1))) // Nominal
        + (((1 & n) * ~0ull) & (val | rbit)));    // Long
}

// Decode using tables when possible, works for all rungs
static std::pair<size_t, uint64_t> qb3dsztbl(uint64_t val, size_t rung) {
    if ((sizeof(DRG) / sizeof(*DRG)) > rung) {
        auto code = DRG[rung][val & ((1ull << (rung + 2)) - 1)];
        return std::make_pair<size_t, uint64_t>(code >> 12, code & TBLMASK);
    }
    return qb3dsz(val, rung);
}

// Decode a B2 sized group of QB3 values from s and acc
// Accumulator should be valid and have at least 56 valid bits
// For rung 0, it works with 17bits or more
// For rung 1, it works with 47bits or more
// returns false on failure
template<typename T>
static bool gdecode(iBits& s, size_t rung, T* group, uint64_t acc, size_t abits) {
    assert(((rung > 1) && (abits <= 8))
        || ((rung == 1) && (abits <= 17)) // B2 + 1
        || ((rung == 0) && (abits <= 47))); // 3 * B2 - 1
    if (0 == rung) { // single bits, direct decoding
        if (0 != (acc & 1)) {
            abits += B2;
            for (size_t i = 0; i < B2; i++) {
                acc >>= 1;
                group[i] = static_cast<T>(1 & acc);
            }
        }
        else
            for (size_t i = 0; i < B2; i++)
                group[i] = static_cast<T>(0);
        s.advance(abits + 1);
        return 1;
    }
    // Table decoding
    if (sizeof(T) == 1 || rung < (sizeof(DRG) / sizeof(*DRG))) {
        if (1 == rung) { // double barrel
            for (size_t i = 0; i < B2; i += 2) {
                auto v = DDRG1[acc & 0x3f];
                group[i] = v & 0x3;
                group[i + 1] = (v >> 2) & 0x3;
                abits += v >> 4;
                acc >>= v >> 4;
            }
            s.advance(abits);
        }
        else if (2 == rung) { // double barrel, max sym len is 4, there are at least 14 in the accumulator
            for (size_t i = 0; i < 14; i += 2) {
                auto v = DDRG2[acc & 0xff];
                group[i] = v & 0x7;
                group[i + 1] = (v >> 3) & 0x7;
                abits += v >> 12;
                acc >>= v >> 12;
            }
            if (abits > 56) { // Rare
                s.advance(abits);
                acc = s.peek();
                abits = 0;
            }
            // last pair
            auto v = DDRG2[acc & 0xff];
            group[14] = v & 0x7;
            group[15] = (v >> 3) & 0x7;
            s.advance(abits + (v >> 12));
        }
        else if (6 > rung) { // Table decode at 3,4 and 5, half of the values per accumulator
            auto drg = DRG[rung];
            const auto m = (1ull << (rung + 2)) - 1;
            for (size_t i = 0; i < B2 / 2; i++) {
                auto v = drg[acc & m];
                abits += v >> 12;
                acc >>= v >> 12;
                group[i] = static_cast<T>(v & TBLMASK);
            }
            s.advance(abits);
            acc = s.peek();
            abits = 0;
            for (size_t i = B2 / 2; i < B2; i++) {
                auto v = drg[acc & m];
                abits += v >> 12;
                acc >>= v >> 12;
                group[i] = static_cast<T>(v & TBLMASK);
            }
            s.advance(abits);
        }
        else { // Last part of table decoding, rungs 6-7, four values per accumulator
            auto drg = DRG[rung];
            const auto m = (1ull << (rung + 2)) - 1;
            for (size_t j = 0; j < B2; j += B2 / 4) {
                for (size_t i = 0; i < B2 / 4; i++) {
                    auto v = drg[acc & m];
                    abits += v >> 12;
                    acc >>= v >> 12;
                    group[j + i] = static_cast<T>(v & TBLMASK);
                }
                s.advance(abits);
                abits = 0;
                if (j <= B2 / 2) // Skip the last peek
                    acc = s.peek();
            }
        }
    }
    else { // computed decoding
        if (sizeof(T) < 8 || rung < 32) { // 16 and 32 bits may reuse accumulator
            for (int i = 0; i < B2; i++) {
                if (abits + rung > 62) {
                    s.advance(abits);
                    acc = s.peek();
                    abits = 0;
                }
                auto p = qb3dsz(acc, rung);
                abits += p.first;
                acc >>= p.first;
                group[i] = static_cast<T>(p.second);
            }
            s.advance(abits);
        }
        else if (rung < 63) { // 64bit and rung in [32 - 62], can't reuse accumulator
            s.advance(abits);
            for (int i = 0; i < B2; i++) {
                auto p = qb3dsz(s.peek(), rung);
                group[i] = static_cast<T>(p.second);
                s.advance(p.first);
            }
        }
        else { // Rung 63 might need 65 bits
            s.advance(abits);
            for (int i = 0; i < B2; i++) {
                auto p = qb3dsz(s.peek(), rung);
                auto ovf = p.first & (p.first >> 6);
                group[i] = static_cast<T>(p.second);
                s.advance(p.first ^ ovf);
                if (ovf) // The next to top bit got dropped, rare
                    group[i] |= s.get() << 62;
            }
        }
    }
    if (0 == (group[B2 - 1] >> rung)) {
        auto stepp = step(group, rung);
        if (stepp < B2)
            group[stepp] ^= static_cast<T>(1ull << rung);
    }
    return true;
}

// Multiply v(in magsign) by m(normal, positive)
template<typename T> static T magsmul(T v, T m) { return magsabs(v) * (m << 1) - (v & 1); }

// reports most but not all errors, for example if the input stream is too short for the last block
template<typename T>
static bool decode(uint8_t *src, size_t len, T* image, const decs &info)
{
    auto xsize(info.xsize), ysize(info.ysize), bands(info.nbands), stride(info.stride);
    auto cband = info.cband;
    static_assert(std::is_integral<T>() && std::is_unsigned<T>(), "Only unsigned integer types allowed");
    constexpr size_t UBITS(sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6);
    constexpr auto NORM_MASK((1ull << UBITS) - 1); // UBITS set
    constexpr auto LONG_MASK(NORM_MASK * 2 + 1); // UBITS + 1 set
    T prev[QB3_MAXBANDS] = {}, pcf[QB3_MAXBANDS] = {}, group[B2] = {};
    size_t runbits[QB3_MAXBANDS] = {};
    const uint16_t* dsw = sizeof(T) == 1 ? dsw3 : sizeof(T) == 2 ? dsw4 : sizeof(T) == 4 ? dsw5 : dsw6;
    stride = stride ? stride : xsize * bands;
    // Set up block offsets based on traversal order, defaults to HILBERT
    uint64_t order(info.order ? info.order : HILBERT);
    size_t offset[B2] = {};
    for (size_t i = 0; i < B2; i++) {
        size_t n = (order >> ((B2 - 1 - i) << 2));
        offset[i] = ((n >> 2) & 0b11) * stride + (n & 0b11) * bands;
    }
    iBits s(src, len);
    bool failed(false);
    for (size_t y = 0; y < ysize; y += B) {
        // If the last row is partial, roll it up
        if (y + B > ysize)
            y = ysize - B;
        for (size_t x = 0; x < xsize; x += B) {
            // If the last column is partial, move it left
            if (x + B > xsize)
                x = xsize - B;
            for (int c = 0; c < bands; c++) {
                failed |= s.empty();
                uint64_t cs(0), abits(1), acc(s.peek());
                if (acc & 1) { // Rung change
                    cs = dsw[(acc >> 1) & LONG_MASK];
                    abits = cs >> 12;
                }
                acc >>= abits;
                if (0 == cs || 0 != (cs & TBLMASK)) { // Normal decoding, not a signal
                    // abits is never > 8, so it's safe to call gdecode
                    auto rung = (runbits[c] + cs) & NORM_MASK;
                    failed |= !gdecode(s, rung, group, acc, abits);
                    runbits[c] = rung;
                }
                else { // extra encoding
                    cs = dsw[acc & LONG_MASK]; // rung, no flag
                    auto rung = (runbits[c] + cs) & NORM_MASK;
                    acc >>= (cs >> 12) - 1; // No flag
                    abits += (cs >> 12) - 1;
                    if (rung != NORM_MASK) { // CF encoding
                        auto cfrung(rung);
                        T cf = pcf[c];
                        auto read_cfr = acc & 1;
                        abits++;
                        acc >>= 1;
                        if (read_cfr) { // different cf, need to read it
                            read_cfr = acc & 1;
                            abits++;
                            acc >>= 1;
                            if (read_cfr) { // has own rung
                                cs = dsw[acc & LONG_MASK];
                                cfrung = (rung + cs) & NORM_MASK;
                                failed |= (cfrung == rung);
                                acc >>= (cs >> 12) - 1;
                                abits += (cs >> 12) - 1;
                            }
                            if (sizeof(T) == 8 && (cfrung + abits) > 62) { // Rare
                                s.advance(abits);
                                acc = s.peek();
                                abits = 0;
                            }
                            auto p = qb3dsztbl(acc, cfrung - read_cfr);
                            pcf[c] = cf = static_cast<T>(p.second + (read_cfr << cfrung));
                            abits += p.first;
                            acc >>= p.first;
                        }
                        cf += 2; // Use it unbiased
                        if (rung) {
                            s.advance(abits);
                            failed |= !gdecode(s, rung, group, s.peek(), 0);
                            // Multiply group by CF and get the max for the actual rung
                            T maxval(group[0] = magsmul(group[0], cf));
                            for (int i = 1; i < B2; i++) {
                                auto val = magsmul(group[i], cf);
                                if (maxval < val) maxval = val;
                                group[i] = val;
                            }
                            failed |= cf > maxval; // Can't be all zero
                            runbits[c] = topbit(maxval | 1);
                        }
                        else { // Single bit for data, decode here
                            if (abits + B2 > 64) {
                                s.advance(abits);
                                acc = s.peek();
                                abits = 0;
                            }
                            T v[2] = { 0, magsmul(T(1), cf) };
                            for (int i = 0; i < B2; i++)
                                group[i] = v[(acc >> i) & 1];
                            s.advance(B2 + abits);
                            runbits[c] = topbit(v[1]);
                        }
                    }
                    else { // IDX decoding
                        cs = dsw[acc & LONG_MASK]; // rung, no flag
                        rung = (runbits[c] + cs) & NORM_MASK;
                        runbits[c] = rung;
                        acc >>= (cs >> 12) - 1; // No flag
                        abits += (cs >> 12) - 1;
                        failed |= rung == 63; // TODO: Deal with 64bit overflow
                        // 16 index values in group, max is 7
                        T maxval(0);
                        for (int i = 0; i < B2; i++) {
                            // Could use ddrg2
                            auto v = DRG[2][acc & 0xf];
                            group[i] = static_cast<uint8_t>(v);
                            if (maxval < group[i])
                                maxval = group[i];
                            acc >>= v >> 12;
                            abits += v >> 12;
                        }
                        s.advance(abits);
                        T idxarray[B2 / 2] = {};
                        for (size_t i = 0; i <= maxval; i++) {
                            acc = s.peek();
                            auto v = qb3dsztbl(acc, rung);
                            s.advance(v.first);
                            idxarray[i] = T(v.second);
                        }
                        for (int i = 0; i < B2; i++)
                            group[i] = idxarray[group[i]];
                    }
                }
                // Undo delta encoding for this block
                auto prv = prev[c];
                T* const blockp = image + y * stride + x * bands + c;
                for (int i = 0; i < B2; i++)
                    blockp[offset[i]] = prv += smag(group[i]);
                prev[c] = prv;
            } // Per band per block
            if (failed) break;
        } // per block
        if (failed) break;
        // For performance apply band delta per block strip, in linear order
        for (size_t j = 0; j < B; j++) {
            for (int c = 0; c < bands; c++) if (c != cband[c]) {
                auto dimg = image + stride * (y + j) + c;
                auto simg = image + stride * (y + j) + cband[c];
                for (int i = 0; i < xsize; i++, dimg += bands, simg += bands)
                    *dimg += *simg;
            }
        }
    } // per block strip
    // It might not catch all errors
    return failed || s.avail() > 7; 
}
} // QB3 namespace
