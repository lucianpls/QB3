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
*/

#pragma once
#include <vector>
#include <cinttypes>
#include <limits>
#include <algorithm>
#include <cassert>
#include <utility>
#include "bitstream.h"
#if defined(_WIN32)
#include <intrin.h>
#endif

// Define this to minimize the size of the code, loosing speed for large data types
// It saves about 20KB, could be useful if only byte data is required
// #define QB3_OPTIMIZE_ONLY_BYTE

namespace QB3 {
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

// Block is 4x4 pixels
constexpr size_t B = 4;
constexpr size_t B2(B* B);

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

//
// Delta and sign reorder
// All these templates work only for T unsigned, integer, 2s complement types
// Even though T is unsigned, it is considered signed, that's the whole point!
//


// Encode integers as absolute magnitude and sign, so the bit 0 is the sign.
// This encoding has the top rung always zero, regardless of sign
// To keep the range exactly the same as two's complement, the magnitude of negative values is biased down by one (no negative zero)


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

// Convert a sequence to mag-sign delta
template<typename T>
static T dsign(T *v, T pred) {
    static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
        "Only works for unsigned integral types");
    for (size_t i = 0; i < B2; i++) {
        pred += v[i] -= pred; // or std::swap(v[i], pred); v[i] = pred - v[i] 
        v[i] = mags(v[i]);
    }
    return pred;
}

// Reverse mag-sign and delta
template<typename T>
static T undsign(T *v, T pred) {
    for (size_t i = 0; i < B2; i++)
        v[i] = pred += smag(v[i]);
    return pred;
}

// Convert from mag-sign to absolute
template<typename T>
inline T revs(T val) {
    return (val >> 1) + (val & 1);
}

// Given mag-sign value, divide by cf
template<typename T>
inline T magsdiv(T val, T cf) {
    T absv = revs(val) / cf; // Integer division
    return (absv << 1) * (1 & ~val) + (((absv - 1) << 1) | 1) * (1 & val);
}

// return greatest common factor (absolute) of a B2 sized vector of mag-sign values
// T is always unsigned
template<typename T>
T gcode(const T* group) {
    // Work with absolute values
    T v[B2];
    size_t sz = 0;
    for (size_t i = 0; i < B2; i++) {
        // ignore the zeros, return early if 1 or -1 are encountered
        if (group[i] > 2) {
            v[sz++] = revs(group[i]);
            continue;
        }
        if (group[i] != 0)
            return 1; // Not useful
    }

    if (0 == sz)
        return 1;

    while (sz > 1) {
        std::swap(v[0], *std::min_element(v, v + sz));
        size_t j = 1;
        const T m = v[0];
        for (size_t i = 1; i < sz; i++) { // Skips the zeros
            if (1 < (v[i] % m)) {
                v[j++] = v[i] % m;
                continue;
            }
            if (1 == (v[i] % m))
                return 1; // Not useful
        }
        sz = j; // Never zero
    }

    return v[0]; // 2 or higher, absolute value
}

// Computed encoding with three codeword lenghts, used for higher rungs
// Yes, it's horrid, but it works fast. Bit fiddling!
// No conditionals, computes all three values and picks one by masking with the condition
// It is faster than similar code with conditions because the calculations for the three lines get interleaved
// The "(~0ull * (1 & <cond>))" is to show the compiler that it is a mask operation
static std::pair<size_t, uint64_t> qb3csz(uint64_t val, size_t rung) {
    uint64_t nxt = (val >> (rung - 1)) & 1;
    uint64_t top = val >> rung;
    return std::make_pair<size_t, uint64_t>(rung + top + (top | nxt),
        +((~0ull * (1 & top)) & (((val ^ (1ull << rung)) >> 2) | ((val & 0b11ull) << rung))) // 1 x BIG     -> 00
        +((~0ull * (1 & ~(top | nxt))) & (val + (1ull << (rung - 1))))                       // 0 0 LITTLE  -> 1?
        +((~0ull * (1 & (~top & nxt))) & (val >> 1 | ((val & 1) << rung))));                 // 0 1 MIDDLE  -> 01
}

static std::pair<size_t, uint64_t> qb3dsz(uint64_t acc, size_t rung) {
    uint64_t ntop = (~(acc >> (rung - 1))) & 1;
    uint64_t rmsk = (1ull << rung) - 1;
    if (1 & ~ntop)
        return std::make_pair(rung, acc & (rmsk >> 1));
    uint64_t nnxt = (~(acc >> (rung - 2))) & 1;
    return std::make_pair(rung + 1 + nnxt,
        (((1 & ~nnxt) * ~0ull) & (((acc << 1) & rmsk) | ((acc >> rung) & 1)))
        + (((1 & nnxt) * ~0ull) & ((rmsk + 1) + ((acc & (rmsk >> 1)) << 2) + ((acc >> rung) & 0b11))));
}

// Try index encoding
// Would save about 1.1% more space, at some speed loss
template<typename T>
size_t trym(const T* group, size_t rung, size_t abits) {
    // Unit size bit length
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;

    std::vector<std::pair<size_t, size_t>> g2(B2);
    g2.clear();
    for (size_t i = 0; i < B2; i++) {
        bool found = false;
        for (auto& p : g2)
            if (p.second == group[i]) {
                p.first++;
                found = true;
            }
        if (!found)
            g2.push_back(std::make_pair(1, group[i]));
    }

    // Normal encoding size
    size_t gsz = abits;
    for (size_t i = 0; i < B2; i++)
        gsz += qb3csz(group[i], rung).first;

    // Index coding size
    size_t g2sz = abits + UBITS; // Signal + real code change
    if (g2.size() != B2) {
        sort(g2.rbegin(), g2.rend()); // High frequency first
        for (auto& p : g2)
            g2sz += qb3csz(p.second, rung).first;
        g2sz += qb3csz(B2 - g2.size(), 3).first; // number of entries saved at rung 3
        size_t nrng = topbit(g2.size());
        // And the indexes
        if (nrng > 0) {
            for (size_t i = 0; i < B2; i++)
                for (size_t j = 0; j < g2.size(); j++)
                    if (group[i] == g2[j].second)
                        g2sz += qb3csz(j, nrng).first;
        }
        else {
            g2sz += B2; // qb3csz doesn't work for rung = 0
        }
    }

    // Common factor encoding, rung change is temporary
    size_t g1sz = UBITS; // Signal
    auto cf = gcode(group);
    if (cf > 1) {
        T v[B2];
        g1sz += qb3csz(0, 3).first; // cf flag
        for (size_t i = 0; i < B2; i++)
            v[i] = magsdiv(group[i], cf);
        auto maxval = *std::max_element(v, v + B2); // Can't be 0
        // Temporary rng
        auto vrng = topbit(maxval);
        // Using a single rung could be wasteful when rung(cf - 2) > rung(maxval)
        // But cf is usually small and we don't need two rungs
        vrng = std::max(vrng, topbit((cf - 2) | 1));
        g1sz += UBITS; // Push vrng as delta
        if (vrng) {
            g1sz += qb3csz(cf - 2, vrng).first;
            for (int i = 0; i < B2; i++)
                g1sz += qb3csz(v[i], vrng).first;
        }
        else { // single bits, cf == 2 or 3
            g1sz += B2 + 2;
        }
        if ((g1sz < gsz) && (g1sz < g2sz))
            return gsz - g1sz;
    }
    return (g2sz < gsz) ? (gsz - g2sz) : 0;
}

// Encode a single group, returns encoded size
template <typename T = uint8_t> size_t group_encode(T group[B2], T maxval, size_t oldrung, oBits &s) {
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    // Encode rung switch using tables, works even with no rung change
    const size_t rung = topbit(maxval | 1); // Force at least one bit set
    uint64_t acc = CSW[UBITS][(rung - oldrung) & ((1ull << UBITS) - 1)];
    size_t abits = acc >> 12;
    acc &= 0xff; // Strip the size
    size_t ssize = s.size();

    if (0 == rung) { // only 1s and 0s, rung is -1 or 0
        acc |= maxval << abits++;
        if (0 != maxval)
            for (int i = 0 ; i < B2; i++)
                acc |= group[i] << abits++;
        s.push(acc, abits);
        return abits;
    }

    // Flip the last set rung bit if the rung bit sequence is a step down
    // At least one rung bit has to be set, so it can't return 0
    if (step(group, rung) <= B2)
        group[step(group, rung) - 1] ^= static_cast<T>(1ull << rung);

    if (6 > rung) { // Encoded data fits in 64 or 128 bits
        auto t = CRG[rung];
        for (size_t i = 0; i < B2 / 2; i++) {
            acc |= (TBLMASK & t[group[i]]) << abits;
            abits += t[group[i]] >> 12;
        }
        // At rung 1 and 2 this push can be skipped, if the accum has enough space
        if (!((rung == 1) || (rung == 2 && abits < 33))) {
            s.push(acc, abits);
            acc = abits = 0;
        }
        for (size_t i = B2 / 2; i < B2; i++) {
            acc |= (TBLMASK & t[group[i]]) << abits;
            abits += t[group[i]] >> 12;
        }
        s.push(acc, abits);
        return s.size() - ssize;
    }

    // Last part of table encoding, rung 6-7 or 6-10
    // Encoded data fits in 256 bits, 4 way interleaved
    if ((sizeof(CRG) / sizeof(*CRG)) > rung) {
        auto t = CRG[rung];
        uint64_t a[4] = { acc, 0, 0, 0 };
        size_t asz[4] = { abits, 0, 0, 0 };
        for (size_t i = 0; i < B; i++)
            for (size_t j = 0; j < B; j++) {
                uint16_t v = t[group[j * B + i]];
                a[j] |= (TBLMASK & v) << asz[j];
                asz[j] += v >> 12;
            }
        for (size_t i = 0; i < B; i++)
            s.push(a[i], asz[i]);
        return s.size() - ssize;
    }

    // Computed encoding, slower, works for rung > 1
    if (1 < sizeof(T)) { // This vanishes in 8 bit mode
        // Push the code switch for non-table encoding, not worth the hassle
        s.push(acc, abits);
        if (63 != rung) {
            for (int i = 0; i < B2; i++) {
                auto p = qb3csz(group[i], rung);
                s.push(p.second, p.first);
            }
        }
        else { // rung 63 may overflow 64 bits, push the second val bit explicitly
            for (int i = 0; i < B2; i++) {
                auto p = qb3csz(group[i], rung);
                size_t ovf = p.first & (p.first >> 6); // overflow flag
                s.push(p.second, p.first ^ ovf); // changes 65 in 64
                if (ovf)
                    s.push(1ull & (group[i] >> 1), ovf);
            }
        }
    }
    return s.size() - ssize;
}

template <typename T = uint8_t>
std::vector<uint8_t> encode_new(const std::vector<T>& image,
    size_t xsize, size_t ysize, int mb = 1)
{
    std::vector<uint8_t> result;
    result.reserve(image.size() * sizeof(T));
    oBits s(result);
    const size_t bands = image.size() / xsize / ysize;
    assert(image.size() == xsize * ysize * bands);
    assert(0 == xsize % B && 0 == ysize % B);

    // Running code length, start with nominal value
    std::vector<size_t> runbits(bands, sizeof(T) * 8 - 1);
    std::vector<T> prev(bands, 0u);      // Previous value, per band
    T group[B2];  // Current 2D group to encode, as array
    size_t offsets[B2];
    for (size_t i = 0; i < B2; i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;
    for (size_t y = 0; y < ysize; y += B) {
        for (size_t x = 0; x < xsize; x += B) {
            size_t loc = (y * xsize + x) * bands; // Top-left pixel address
            for (size_t c = 0; c < bands; c++) { // blocks are band interleaved
                // Collect the block for this band
                if (mb >= 0 && mb < bands && mb != c)
                    for (size_t i = 0; i < B2; i++)
                        group[i] = image[loc + offsets[i] + c] - image[loc + offsets[i] + mb];
                else
                    for (size_t i = 0; i < B2; i++)
                        group[i] = image[loc + offsets[i] + c];

                // Delta in low sign group encode
                prev[c] = dsign(group, prev[c]);
                const T maxval = *std::max_element(group, group + B2);
                group_encode(group, maxval, runbits[c], s);
                runbits[c] = topbit(1ull | maxval);
            }
        }
    }
    return result;
}

template <typename T = uint8_t>
std::vector<uint8_t> encode(const std::vector<T>& image,
    size_t xsize, size_t ysize, int mb = 1)
{
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    std::vector<uint8_t> result;
    result.reserve(image.size() * sizeof(T));
    oBits s(result);
    const size_t bands = image.size() / xsize / ysize;
    assert(image.size() == xsize * ysize * bands);
    assert(0 == xsize % B && 0 == ysize % B);

    // Running code length, start with nominal value
    std::vector<size_t> runbits(bands, sizeof(T) * 8 - 1);
    std::vector<T> prev(bands, 0u);      // Previous value, per band
    T group[B2];  // Current 2D group to encode, as array
    size_t offsets[B2];
    size_t count = 0;
    for (size_t i = 0; i < B2; i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;
    for (size_t y = 0; y < ysize; y += B) {
        for (size_t x = 0; x < xsize; x += B) {
            size_t loc = (y * xsize + x) * bands; // Top-left pixel address
            for (size_t c = 0; c < bands; c++) { // blocks are band interleaved

                // Collect the block for this band
                if (mb >= 0 && mb < bands && mb != c)
                    for (size_t i = 0; i < B2; i++)
                        group[i] = image[loc + offsets[i] + c] - image[loc + offsets[i] + mb];
                else
                    for (size_t i = 0; i < B2; i++)
                        group[i] = image[loc + offsets[i] + c];

                // Delta in low sign group encode
                prev[c] = dsign(group, prev[c]);
                const uint64_t maxval = *std::max_element(group, group + B2);
                const size_t rung = topbit(maxval | 1); // Force at least one bit set

                // Encode rung switch using tables, works even with no rung change
                uint64_t acc = CSW[UBITS][(rung - runbits[c]) & ((1ull << UBITS) - 1)];
                size_t abits = acc >> 12;
                acc &= 0xff; // Strip the size
                runbits[c] = rung;

                if (0 == rung) { // only 1s and 0s, rung is -1 or 0
                    acc |= maxval << abits++;
                    if (0 != maxval)
                        for (uint64_t v : group)
                            acc |= v << abits++;
                    s.push(acc, abits);
                    continue;
                }

                // Flip the last set rung bit if the rung bit sequence is a step down
                // At least one rung bit has to be set, so it can't return 0
                if (step(group, rung) <= B2)
                    group[step(group, rung) - 1] ^= static_cast<T>(1ull << rung);

                //count += trym(group, rung, abits);

                if (6 > rung) { // Encoded data fits in 64 or 128 bits
                    auto t = CRG[rung];
                    for (size_t i = 0; i < B2 / 2; i++) {
                        acc |= (TBLMASK & t[group[i]]) << abits;
                        abits += t[group[i]] >> 12;
                    }
                    // At rung 1 and 2 this push can be skipped, if the accum has enough space
                    if (!((rung == 1) || (rung == 2 && abits < 33))) {
                        s.push(acc, abits);
                        acc = abits = 0;
                    }
                    for (size_t i = B2 / 2; i < B2; i++) {
                        acc |= (TBLMASK & t[group[i]]) << abits;
                        abits += t[group[i]] >> 12;
                    }
                    s.push(acc, abits);
                    continue;
                }

                // Last part of table encoding, rung 6-7 or 6-10
                // Encoded data fits in 256 bits, 4 way interleaved
                if ((sizeof(CRG)/sizeof(*CRG)) > rung) {
                    auto t = CRG[rung];
                    uint64_t a[4] = { acc, 0, 0, 0 };
                    size_t asz[4] = { abits, 0, 0, 0 };
                    for (size_t i = 0; i < B ; i++)
                        for (size_t j = 0; j < B; j++) {
                            uint16_t v = t[group[j * B + i]];
                            a[j] |= (TBLMASK & v) << asz[j];
                            asz[j] += v >> 12;
                        }
                    for (size_t i = 0; i < B; i++)
                        s.push(a[i], asz[i]);
                    continue;
                }

                // Computed encoding, slower, works for rung > 1
                if (1 < sizeof(T)) { // This vanishes in 8 bit mode
                    // Push the code switch for non-table encoding, not worth the hassle
                    s.push(acc, abits);
                    if (63 != rung) {
                        for (uint64_t val : group) {
                            auto p = qb3csz(val, rung);
                            s.push(p.second, p.first);
                        }
                    }
                    else { // rung 63 may overflow 64 bits, push the second val bit explicitly
                        for (uint64_t val : group) {
                            auto p = qb3csz(val, rung);
                            size_t ovf = p.first & (p.first >> 6); // overflow flag
                            s.push(p.second, p.first ^ ovf); // changes 65 in 64
                            s.push(ovf & (val >> 1), ovf); // If value is 0, fine to call with nbits == 0
                        }
                    }
                }
            }
        }
    }
    if (count)
        printf("Saved %d bytes\n", int(count / 8));
    return result;
}

template<typename T = uint8_t>
std::vector<T> decode(std::vector<uint8_t>& src,
    size_t xsize, size_t ysize, size_t bands, int mb = 1)
{
    // Unit size bit length
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;

    std::vector<T> image(xsize * ysize * bands);
    iBits s(src);
    std::vector<T> prev(bands, 0);
    T group[B2];
    std::vector<size_t> runbits(bands, sizeof(T) * 8 - 1);

    std::vector<size_t> offsets(B2);
    for (size_t i = 0; i < B2; i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;

    uint64_t acc;
    size_t abits = 0;
    for (size_t y = 0; (y + B) <= ysize; y += B) {
        for (size_t x = 0; (x + B) <= xsize; x += B) {
            size_t loc = (y * xsize + x) * bands;
            for (int c = 0; c < bands; c++) {
                acc = s.peek();
                abits = 1; // Used bits
                if (acc & 1) {
                    auto cs = DSW[UBITS][(acc >> 1) & ((1ull << (UBITS + 1)) - 1)];
                    runbits[c] = (runbits[c] + cs) & ((1ull << UBITS) - 1);
                    abits = static_cast<size_t>(cs >> 12);
                }
                const size_t rung = runbits[c];

                if (0 == rung) { // single bits, special case
                    if (0 != ((acc >> abits++) & 1)) {
                        acc >>= abits;
                        abits += B2;
                        for (auto& v : group) {
                            v = static_cast<T>(acc & 1);
                            acc >>= 1;
                        }
                    }
                    else
                        for (auto& v : group)
                            v = static_cast<T>(0);
                    s.advance(abits);
                    goto GROUP_DONE;
                }

                if (rung < 6) { // Table decode, half of the values fit in accumulator
                    const auto drg = DRG[rung];
                    const auto m = (1ull << (rung + 2)) - 1;
                    for (size_t i = 0; i < B2 / 2; i++) {
                        auto v = drg[(acc >> abits) & m];
                        abits += v >> 12;
                        group[i] = static_cast<T>(v & TBLMASK);
                    }
                    // Skip the peek if we have enough bits in accumulator
                    if (!((rung == 1) || (rung == 2 && abits < 33))) {
                        s.advance(abits);
                        acc = s.peek();
                        abits = 0;
                    }
                    for (size_t i = B2 / 2; i < B2; i++) {
                        auto v = drg[(acc >> abits) & m];
                        abits += v >> 12;
                        group[i] = static_cast<T>(v & TBLMASK);
                    }
                    s.advance(abits);
                    goto GROUP_DONE;
                }

                // Last part of table decoding, use the accumulator for every 4 values
                if ((sizeof(DRG) / sizeof(*DRG)) > rung) {
                    const auto drg = DRG[rung];
                    const auto m = (1ull << (rung + 2)) - 1;
                    for (size_t j = 0; j < B2; j += B2 / 4) {
                        for (size_t i = 0; i < B2 / 4; i++) {
                            auto v = drg[(acc >> abits) & m];
                            abits += v >> 12;
                            group[j + i] = static_cast<T>(v & TBLMASK);
                        }
                        s.advance(abits);
                        acc = s.peek();
                        abits = 0;
                    }
                    goto GROUP_DONE;
                }

                // Computed decoding, with single stream read
                s.advance(abits);

                if (sizeof(T) != 8) {
                    for (auto& it : group) {
                        auto p = qb3dsz(s.peek(), rung);
                        it = static_cast<T>(p.second);
                        s.advance(p.first);
                    }
                }
                else { // Only for 64bit data
                    if (63 != rung) {
                        for (auto& it : group) {
                            auto p = qb3dsz(s.peek(), rung);
                            it = static_cast<T>(p.second);
                            s.advance(p.first);
                        }
                    }
                    else { // Might overflow
                        for (auto& it : group) {
                            auto p = qb3dsz(s.peek(), rung);
                            auto ovf = p.first & (p.first >> 6);
                            it = static_cast<T>(p.second);
                            s.advance(p.first - ovf);
                            if (ovf)
                                it |= s.get() << 1;
                        }
                    }
                }
GROUP_DONE:
                // Undo the step shift, MSB of last value has to be zero
                if ((0 == (group[B2 - 1] >> rung)) & (rung > 0)) {
                    auto p = step(group, rung);
                    assert(p != B2); // Can't occur, could be a signal
                    if (p < B2)
                        group[p] ^= static_cast<T>(1) << rung;
                }
                prev[c] = undsign(group, prev[c]);
                for (size_t i = 0; i < B2; i++)
                    image[loc + c + offsets[i]] = group[i];
            }
            for (int c = 0; c < bands; c++)
                if (mb >= 0 && mb != c)
                    for (size_t i = 0; i < B2; i++)
                        image[loc + c + offsets[i]] += image[loc + mb + offsets[i]];
        }
    }
    return image;
}

} // Namespace
