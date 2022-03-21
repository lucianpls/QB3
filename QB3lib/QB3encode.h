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

Content: QB3 encoding

Contributors:  Lucian Plesea
*/

#pragma once
#include <vector>
#include <utility>
#include <functional>
#include <algorithm>
#include <type_traits>
#include "bitstream.h"
#include "QB3common.h"

namespace QB3 {
// integer divide val(in magsign) by cf(normal)
template<typename T> static T magsdiv(T val, T cf) {return ((magsabs(val) / cf) << 1) - (val & 1);}

// return greatest common factor (absolute) of a B2 sized vector of mag-sign values
// T is always unsigned
template<typename T>
static T gcf(const T* group) {
    // Work with actual absolute values
    T v[B2] = {0}; // Only the first value has to be zero
    int sz = 0;
    for (int i = 0; i < B2; i++) { // skip the zeros
        if (group[i] > 2)
            v[sz++] = magsabs(group[i]); 
        else if (group[i] != 0) // 0b01 and 0b10 in mags are + or - 1
            return 1; // No common factor
    }
    while (sz > 1) {
        std::swap(v[0], *std::min_element(v, v + sz));
        int j = 1;
        const T m = v[0];
        for (int i = 1; i < sz; i++) { // skip the zeros
            if (1 < v[i] % m)
                v[j++] = v[i] % m;
            else if (1 == v[i] % m)
                return 1; // No common factor
        }
        sz = j; // Never zero
    }
    return v[0]; // 2 or higher if there is a common factor
}

// Computed encoding with three codeword lenghts, used for higher rungs
// No conditionals, computes all three values and picks one by masking with the condition
// It is faster than similar code with conditions because the calculations for the three lines get interleaved
// The "(~0ull * (1 & <cond>))" is to show the compiler that it is a mask operation
static std::pair<size_t, uint64_t> qb3csz(uint64_t val, size_t rung) {
    assert(rung > 1); // Works for rungs 2+
    uint64_t nxt = (val >> (rung - 1)) & 1;
    uint64_t top = val >> rung;
    return std::make_pair<size_t, uint64_t>(rung + top + (top | nxt),
        + ((~0ull * (1 & top)) & (((val ^ (1ull << rung)) >> 2) | ((val & 0b11ull) << rung))) // 1 x LONG     -> 00
        + ((~0ull * (1 & ~(top | nxt))) & (val + (1ull << (rung - 1))))                       // 0 0 SHORT    -> 1x
        + ((~0ull * (1 & (~top & nxt))) & (val >> 1 | ((val & 1) << rung))));                 // 0 1 NOMINAL  -> 01
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
static void groupencode(T group[B2], T maxval, oBits& s,
    uint64_t acc = 0, size_t abits = 0)
{
    assert(abits <= 64);
    if (abits > 8) { // Just in case, a rung switch is 8 bits at most
        s.push(acc, abits);
        acc = abits = 0;
    }
    const size_t rung = topbit(maxval | 1); // Force at least one bit set
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
    if (step(group, rung) <= B2) {
        assert(step(group, rung) > 0); // At least one rung bit should be set
        group[step(group, rung) - 1] ^= static_cast<T>(1ull << rung);
    }
    if (6 > rung) { // Half of the group fits in 64 bits
        auto t = CRG[rung];
        for (size_t i = 0; i < B2 / 2; i++) {
            acc |= (TBLMASK & t[group[i]]) << abits;
            abits += t[group[i]] >> 12;
        }
        // At rung 1, 2 and 3 this push can be skipped, if the accum has enough space
        if (!((rung == 1) || (rung == 2 && abits < 33))) {
            s.push(acc, abits);
            acc = abits = 0;
        }
        for (size_t i = B2 / 2; i < B2; i++) {
            acc |= (TBLMASK & t[group[i]]) << abits;
            abits += t[group[i]] >> 12;
        }
        s.push(acc, abits);
        return;
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
        return;
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
}

// Base QB3 group encode with code switch, returns encoded size
template <typename T = uint8_t> 
static void groupencode(T group[B2], T maxval, size_t oldrung, oBits& s) {
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    uint64_t acc = CSW[UBITS][(topbit(maxval | 1) - oldrung) & ((1ull << UBITS) - 1)];
    groupencode(group, maxval, s, acc & TBLMASK, static_cast<size_t>(acc >> 12));
}

// Group encode with cf
template <typename T = uint8_t>
static void cfgenc(oBits &bits, T group[B2], T cf, size_t oldrung) {
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    uint64_t acc = SIGNAL[UBITS] & TBLMASK;
    size_t abits = UBITS + 2; // SIGNAL[UBITS] >> 12;
    // divide group values by CF and find the new maxvalue
    T maxval = 0;
    for (size_t i = 0; i < B2; i++) if (group[i])
        maxval = std::max(maxval, group[i] = T(((magsabs(group[i]) / cf) << 1) - (group[i] & 1)));
    cf -= 2; // Bias down, 0 and 1 are not used
    auto trung = topbit(maxval | 1); // cf mode rung
    auto cfrung = topbit(cf | 1); // rung for cf-2 value
    // Encode the trung, with or without switch
    // Use the wrong way switch for in-band
    auto cs = CSW[UBITS][(trung - oldrung) & ((1ull << UBITS) - 1)];
    if ((cs >> 12) == 1) // Would be no-switch, use signal instead, it decodes to delta of zero
        cs = SIGNAL[UBITS];
    // When trung is only somewhat larger than cfrung encode cf at same rung as data
    if (trung >= cfrung && (trung < (cfrung + UBITS) || 0 == cfrung)) {
        acc |= (cs & TBLMASK) << abits;
        abits += cs >> 12;
        if (trung == 0) { // Special encoding for single bit
            // maxval can't be zero, so we don't use the all-zeros flag
            // But we do need to save the CF bit
            acc |= static_cast<uint64_t>(cf) << abits++;
            // And the group bits
            for (int i = 0; i < B2; i++)
                acc |= static_cast<uint64_t>(group[i]) << abits++;
            // store it directly in the main output stream
            bits.push(acc, abits);
            return; // done
        }
        cfrung = trung; // Encode cf value with trung
        // Push the accumulator and the cf encoding, cfrung can't be zero or 63
        auto p = qb3csztbl(cf, cfrung);
        if (p.first + abits <= 64) {
            acc |= p.second << abits;
            abits += p.first;
        }
        else { // can't oveflow since cfrung can't be 63
            bits.push(acc, abits);
            acc = p.second;
            abits = p.first;
        }
    }
    else { // CF needs a different rung than the group, so the change is never 0
        // First, encode trung using code-switch with the change bit cleared
        acc |= (cs & (TBLMASK - 1)) << abits;
        abits += cs >> 12;
        // Then encode cfrung, using code-switch from trung, 
        // skip the change bit, since rung will always be different
        // cfrung - trung is never 0
        cs = CSW[UBITS][(cfrung - trung) & ((1ull << UBITS) - 1)];
        acc |= (cs & (TBLMASK - 1)) << (abits - 1);
        abits += static_cast<size_t>(cs >> 12) - 1;
        if (cfrung > 1) {
            auto p = qb3csztbl(cf ^ (1ull << cfrung), cfrung - 1); // Can't overflow
            if (p.first + abits <= 64) {
                acc |= p.second << abits;
                abits += p.first;
            }
            else { // can't oveflow since cfrung can't be 63
                bits.push(acc, abits);
                acc = p.second;
                abits = p.first;
            }
        }
        else { // single bit, there is enough space, cfrung 0 or 1, save only the bottom bit
            acc |= static_cast<uint64_t>(cf - static_cast<T>(cfrung * 2)) << abits++;
        }
    }
    groupencode(group, maxval, bits, acc, abits);
}

// Round to Zero Division, no overflow
template<typename T>
static T rto0div(T x, T y) {
    static_assert(std::is_integral<T>(), "Integer types only");
    T r = x / y, m = x % y;
    y = (y >> 1);
    return r + (~(x < 0) & (m > y)) - ((x < 0) & (m < -y));
}

// Round from Zero Division, no overflow
template<typename T>
static T rfr0div(T x, T y) {
    static_assert(std::is_integral<T>(), "Integer types only");
    T r = x / y, m = x % y;
    y = (y >> 1) + (y & 1);
    return r + (~(x < 0) & (m >= y)) - ((x < 0) & (m <= -y));
}

// Use objects instead of raw functions
// The object version is slower, measurably so, but allows more flexibility features.
#define OBJ_
#if defined(OBJ_)

template<typename T>
struct encoder {
    encoder(oBits &s) : _s(s) {};
    virtual ~encoder() {};
    oBits& _s;
    T const* *line;
    size_t* runbits;
    T* prev;
    size_t* core;
    T group[B2];
    size_t xsz, ysz, csz;

    T collect_group() {
        auto prv = prev[c];
        T maxval(0);
        if (c != core[c]) {
            for (int i = 0; i < B2; i++) {
                const T* pixl = line[y + ylut[i]] + (x + xlut[i]) * csz;
                T g = pixl[c] - pixl[core[c]];
                prv += g -= prv;
                group[i] = mags(g);
                maxval = std::max(maxval, mags(g));
            }
        }
        else {
            for (int i = 0; i < B2; i++) {
                T g = (line[y + ylut[i]] + (x + xlut[i]) * csz)[c];
                prv += g -= prv;
                group[i] = mags(g);
                maxval = std::max(maxval, mags(g));
            }
        }
        prev[c] = prv;
        return maxval;
    }

    // Same as above, but pre-divide the inputs by a factor
    T collect_div(T factor) {
        auto prv = prev[c];
        T maxval(0);
        if (c != core[c]) {
            for (int i = 0; i < B2; i++) {
                const T* pixl = line[y + ylut[i]] + (x + xlut[i]) * csz;
                T g = pixl[c] - pixl[core[c]];
                prv += g -= prv;
                group[i] = mags(g);
                maxval = std::max(maxval, mags(g));
            }
        }
        else {
            for (int i = 0; i < B2; i++) {
                T g = (line[y + ylut[i]] + (x + xlut[i]) * csz)[c];
                prv += g -= prv;
                group[i] = mags(g);
                maxval = std::max(maxval, mags(g));
            }
        }
        prev[c] = prv;
        return maxval;
    }

    virtual void encode_strip(); // Assumes c and x info is valid
    void encode(); // Assumes c, x and y info is valid
    int encode_image(const T* image, size_t xsize, size_t ysize, size_t bands, const size_t* cband);

    // block location
    size_t y; // Top line
    size_t x; // Left column
    size_t c; // band

    // The variables named without an _ are raw pointers to the data of these vectors.
    // Even in release mode, accessing a vector content is slower than a raw pointer
    std::vector<size_t> _runbits;
    std::vector<T> _prev;
    std::vector<size_t> _core;
    std::vector<T const*> _line;
};

// fast encoding
template<typename T>
void encoder<T>::encode_strip() {
    assert(0 == xsz % B);
    for (x = 0; x < xsz; x += B) {
        for (c = 0; c < csz; c++) {
            T maxval = collect_group();
            groupencode(group, maxval, runbits[c], _s);
            runbits[c] = topbit(maxval | 1);
        }
    }
}

// Assumes all lines are valid
template<typename T>
void encoder<T>::encode() {
    assert(0 == ysz % B);
    for (y = 0; y < ysz; y += B)
        encode_strip();
}

// Checks input, sets up the state then calls encode
template<typename T>
int encoder<T>::encode_image(const T* image, size_t xsize, size_t ysize, size_t bands, const size_t* cband) {
    if (xsize == 0 || xsize > 0x10000ull || ysize == 0 || ysize > 0x10000ull
        || 0 == bands || nullptr == cband)
        return 1;
    // Check band mapping
    for (size_t c = 0; c < bands; c++)
        if (cband[c] >= bands)
            return 2; // Band mapping error
    if (0 != ((xsize % B) | (ysize % B)))
        return 3; // Error, size is not a block multiple
    csz = bands;
    ysz = ysize;
    xsz = xsize;
    _core.resize(bands);
    _prev.resize(bands, 0);
    _runbits.resize(bands, 0);
    for (c = 0; c < bands; c++)
        _core[c] = cband[c];
    _line.resize(ysz);
    size_t line_size = bands * xsz;
    for (y = 0; y < ysz; y++)
        _line[y] = image + y * line_size;
    line = _line.data();
    core = _core.data();
    prev = _prev.data();
    runbits = _runbits.data();
    encode();
    return 0;
}

template<typename T>
struct encoder_best : encoder<T> {
    encoder_best(oBits& s) : encoder<T>(s) {};
    virtual ~encoder_best() {};
    virtual void encode_strip();
};

template<typename T>
void encoder_best<T>::encode_strip() {
    for (this->x = 0; this->x < this->xsz; this->x += B) {
        for (this->c = 0; this->c < this->csz; this->c++) {
            T maxval = this->collect_group();
            const auto oldrung = this->runbits[this->c];
            auto rung = topbit(maxval | 1);
            this->runbits[this->c] = rung;
            if (rung != 0) {
                auto cf = gcf(this->group);
                if (cf > 1) {// Always smaller in cf encoding
                    cfgenc(this->_s, this->group, cf, oldrung);
                    continue;
                }
            }
            groupencode(this->group, maxval, oldrung, this->_s);
        }
    }
}

template<typename T>
static int encode_fast(const T* image, oBits& s, size_t xsize, size_t ysize, size_t bands, const size_t* cband)
{
    encoder<T> encdr(s);
    return encdr.encode_image(image, xsize, ysize, bands, cband);
}

template<typename T>
static int encode_best(const T* image, oBits& s, size_t xsize, size_t ysize, size_t bands, const size_t* cband)
{
    encoder_best<T> encdr(s);
    return encdr.encode_image(image, xsize, ysize, bands, cband);
}

#else

// Encode a whole image without using encoder objects
// These are faster but less flexible
 
// Only basic encoding
template<typename T>
static int encode_fast(const T* image, oBits& s, size_t xsize, size_t ysize, size_t bands, const size_t* cband)
{
    static_assert(std::is_integral<T>() && std::is_unsigned<T>(), "Only unsigned integer types allowed");
    if (xsize == 0 || xsize > 0x10000ull || ysize == 0 || ysize > 0x10000ull
        || 0 == bands || nullptr == cband)
        return 1;
    // Check band mapping
    for (size_t c = 0; c < bands; c++)
        if (cband[c] >= bands)
            return 2; // Band mapping error
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    // Running code length, start with nominal value
    std::vector<size_t> _runbits(bands, 0);
    auto runbits = _runbits.data();
    std::vector<T> _prev(bands, T(0));      // Previous value, per band
    auto prev = _prev.data();
    size_t offsets[B2];
    for (size_t i = 0; i < B2; i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;
    T group[B2];
    for (size_t y = 0; y < ysize; y += B) {
        for (size_t x = 0; x < xsize; x += B) {
            const size_t loc = (y * xsize + x) * bands; // Top-left pixel address
            for (size_t c = 0; c < bands; c++) { // blocks are band interleaved
                T maxval(0); // Maximum mag-sign value within this group
                // Collect the block for this band, convert to running delta mag-sign
                auto prv = prev[c];
                // Use separate loop for basebands to avoid a test inside the hot loop
                if (c != cband[c]) {
                    for (size_t i = 0; i < B2; i++) {
                        T g = image[loc + c + offsets[i]] - image[loc + cband[c] + offsets[i]];
                        prv += g -= prv;
                        group[i] = mags(g);
                        maxval = std::max(maxval, mags(g));
                    }
                }
                else { // baseband
                    for (size_t i = 0; i < B2; i++) {
                        T g = image[loc + c + offsets[i]];
                        prv += g -= prv;
                        group[i] = mags(g);
                        maxval = std::max(maxval, mags(g));
                    }
                }
                prev[c] = prv;
                groupencode(group, maxval, runbits[c], s);
                runbits[c] = topbit(maxval | 1);
            }
        }
    }
    return 0;
}

// Returns error code or 0 if success
// TODO: Error code mapping
template <typename T = uint8_t>
static int encode_best(const T *image, oBits& s, size_t xsize, size_t ysize, size_t bands, const size_t *cband)
{
    static_assert(std::is_integral<T>() && std::is_unsigned<T>(), "Only unsigned integer types allowed");
    if (xsize == 0 || xsize > 0x10000ull || ysize == 0 || ysize > 0x10000ull
        || 0 == bands || nullptr == cband)
        return 1;
    // Check band mapping
    for (size_t c = 0; c < bands; c++)
        if (cband[c] >= bands)
            return 2; // Band mapping error
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    // Running code length, start with nominal value
    std::vector<size_t> _runbits(bands, 0);
    auto runbits = _runbits.data();
    std::vector<T> _prev(bands, 0u); // Previous value, per band
    auto prev = _prev.data();
    T group[B2]; // Current 2D group to encode, as array
    size_t offsets[B2];
    for (size_t i = 0; i < B2; i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;
    for (size_t y = 0; y < ysize; y += B) {
        for (size_t x = 0; x < xsize; x += B) {
            size_t loc = (y * xsize + x) * bands; // Top-left pixel address
            for (size_t c = 0; c < bands; c++) { // blocks are always band interleaved
                T maxval(0); // Maximum mag-sign value within this group
                // Collect the block for this band, convert to running delta mag-sign
                auto prv = prev[c];
                if (c != cband[c]) {
                    for (size_t i = 0; i < B2; i++) {
                        T g = image[loc + c + offsets[i]] - image[loc + cband[c] + offsets[i]];
                        prv += g -= prv;
                        group[i] = mags(g);
                        maxval = std::max(maxval, mags(g));
                    }
                }
                else {
                    for (size_t i = 0; i < B2; i++) {
                        T g = image[loc + c + offsets[i]];
                        prv += g -= prv;
                        group[i] = mags(g);
                        maxval = std::max(maxval, mags(g));
                    }
                }
                prev[c] = prv;
                auto oldrung = runbits[c];
                const size_t rung = topbit(maxval | 1); // Force at least one bit set
                runbits[c] = rung;
                if (0 == rung) { // only 1s and 0s, rung is -1 or 0
                    // Encode as QB3 group, no point in trying cf
                    uint64_t acc = CSW[UBITS][(rung - oldrung) & ((1ull << UBITS) - 1)];
                    size_t abits = acc >> 12;
                    acc &= 0xffull;
                    acc |= static_cast<uint64_t>(maxval) << abits++; // Add the all-zero flag
                    if (0 != maxval)
                        for (size_t i = 0; i < B2; i++)
                            acc |= static_cast<uint64_t>(group[i]) << abits++;
                    s.push(acc, abits);
                    continue;
                }
                auto cf = gcf(group);
                if (cf > 1) // Always smaller in cf encoding
                    cfgenc(s, group, cf, oldrung);
                else
                    groupencode(group, maxval, oldrung, s);
            }
        }
    }
    return 0;
}

#endif // OBJ_

} // namespace