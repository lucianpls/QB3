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
#include "bitstream.h"

//#define HISTOGRAM
//#include <map>

namespace QB3 {
#include "QB3common.h"

// Convert from mag-sign to absolute
template<typename T>
static inline T revs(T val) {
    return (val >> 1) + (val & 1);
}

// integer divide val(in magsign) by cf(normal)
template<typename T>
static inline T magsdiv(T val, T cf) {
    T absv = revs(val) / cf; // Integer division
    return ((absv << 1) & (~T(0) * (~val & 1))) + ((((absv - 1) << 1) | 1) & (~T(0) * (val & 1)));
}

// integer multiply val(in magsign) by cf(normal)
template<typename T>
static inline T magsmul(T val, T cf) {
    T absv = revs(val) * cf;
    return (absv << 1) * (~val & 1) + (((absv - 1) << 1) | 1) * (val & 1);
}

// return greatest common factor (absolute) of a B2 sized vector of mag-sign values
// T is always unsigned
template<typename T>
T gcode(const T* group) {
    // Work with absolute values
    T v[B2];
    size_t sz = 0;
    for (size_t i = 0; i < B2; i++) {
        // skip the zeros, return early if 1 or -1 are encountered
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
    assert(rung > 1); // Does not work for rungs 0 or 1
    uint64_t nxt = (val >> (rung - 1)) & 1;
    uint64_t top = val >> rung;
    return std::make_pair<size_t, uint64_t>(rung + top + (top | nxt),
        +((~0ull * (1 & top)) & (((val ^ (1ull << rung)) >> 2) | ((val & 0b11ull) << rung))) // 1 x BIG     -> 00
        + ((~0ull * (1 & ~(top | nxt))) & (val + (1ull << (rung - 1))))                       // 0 0 LITTLE  -> 1?
        + ((~0ull * (1 & (~top & nxt))) & (val >> 1 | ((val & 1) << rung))));                 // 0 1 MIDDLE  -> 01
}

// Single value QB3 encode, works for rungs 1 and above
static std::pair<size_t, uint64_t> qb3csztbl(uint64_t val, size_t rung) {
    assert(rung);
    if ((sizeof(CRG) / sizeof(*CRG)) > rung) {
        auto cs = CRG[rung][val];
        return std::make_pair<size_t, uint64_t>(cs >> 12, cs & TBLMASK);
    }
    return qb3csz(val, rung);
}

// TODO: See if it makes any difference
//// Encode a single value, using tables if needed, at any rung between 1 and 63
//// Faster than qb3csz for small values, likely not worth the effort
//static std::pair<size_t, uint64_t> qb3code(uint64_t val, size_t rung) {
//    assert(rung != 0); // Don't use for rung 0
//    // Use tables
//    if ((sizeof(CRG) / sizeof(*CRG)) > rung) {
//        auto code = CRG[rung][val];
//        return std::make_pair<size_t, uint64_t>(code >> 12, code & TBLMASK);
//    }
//    return qb3csz(val, rung);
//}

//template<typename T>
//size_t trym(const T* group, size_t rung, size_t abits) {
//    // Unit size bit length
//    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
//
//    std::vector<std::pair<size_t, size_t>> g2(B2);
//    g2.clear();
//    for (size_t i = 0; i < B2; i++) {
//        bool found = false;
//        for (auto& p : g2)
//            if (p.second == group[i]) {
//                p.first++;
//                found = true;
//            }
//        if (!found)
//            g2.push_back(std::make_pair(1, group[i]));
//    }
//
//    // Normal encoding size
//    size_t gsz = abits;
//    for (size_t i = 0; i < B2; i++)
//        gsz += qb3csz(group[i], rung).first;
//
//    // Index coding size
//    size_t g2sz = abits + UBITS; // Signal + real code change
//    if (g2.size() != B2) {
//        sort(g2.rbegin(), g2.rend()); // High frequency first
//        for (auto& p : g2)
//            g2sz += qb3csz(p.second, rung).first;
//        g2sz += qb3csz(B2 - g2.size(), 3).first; // number of entries saved at rung 3
//        size_t nrng = topbit(g2.size());
//        // And the indexes
//        if (nrng > 0) {
//            for (size_t i = 0; i < B2; i++)
//                for (size_t j = 0; j < g2.size(); j++)
//                    if (group[i] == g2[j].second)
//                        g2sz += qb3csz(j, nrng).first;
//        }
//        else {
//            g2sz += B2; // qb3csz doesn't work for rung = 0
//        }
//    }
//
//    // Common factor encoding, rung change is temporary
//    size_t g1sz = UBITS; // Signal
//    auto cf = gcode(group);
//    if (cf > 1) {
//        T v[B2];
//        g1sz += qb3csz(0, 3).first; // cf flag
//        for (size_t i = 0; i < B2; i++)
//            v[i] = magsdiv(group[i], cf);
//        auto maxval = *std::max_element(v, v + B2); // Can't be 0
//        // Temporary rng
//        auto vrng = topbit(maxval);
//        // Using a single rung could be wasteful when rung(cf - 2) > rung(maxval)
//        // But cf is usually small and we don't need two rungs
//        vrng = std::max(vrng, topbit((cf - 2) | 1));
//        g1sz += UBITS; // Push vrng as delta
//        if (vrng) {
//            g1sz += qb3csz(cf - 2, vrng).first;
//            for (int i = 0; i < B2; i++)
//                g1sz += qb3csz(v[i], vrng).first;
//        }
//        else { // single bits, cf == 2 or 3
//            g1sz += B2 + 2;
//        }
//        if ((g1sz < gsz) && (g1sz < g2sz))
//            return gsz - g1sz;
//    }
//    return (g2sz < gsz) ? (gsz - g2sz) : 0;
//}

// only encode the group entries, not the rung switch
// maxval is used to choose the rung for encoding
// If abits > 0, the accumulator is also pushed into the stream
template <typename T> size_t groupencode(T group[B2], T maxval, oBits& s,
    uint64_t acc = 0, size_t abits = 0)
{
    size_t ssize = s.size();
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
        return abits;
    }

    // Flip the last set rung bit if the rung bit sequence is a step down
    // At least one rung bit has to be set, so it can't return 0
    if (step(group, rung) <= B2) {
        assert(step(group, rung) > 0); // At least one rung bit should be set
        group[step(group, rung) - 1] ^= static_cast<T>(1ull << rung);
    }

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

// Base QB3 group encode with code switch, returns encoded size
template <typename T = uint8_t> size_t groupencode(T group[B2], T maxval, size_t oldrung, oBits& s) {
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    // Encode rung switch using tables, works even with no rung change
    const size_t rung = topbit(maxval | 1); // Force at least one bit set
    uint64_t acc = CSW[UBITS][(rung - oldrung) & ((1ull << UBITS) - 1)];
    return groupencode(group, maxval, s, acc & 0xffull, static_cast<size_t>(acc >> 12));
}


// Group encode with cf
template <typename T = uint8_t>
void cfgenc(oBits &bits, T group[B2], T cf, size_t oldrung)
{
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    uint64_t acc = SIGNAL[UBITS] & 0xff;
    size_t abits = SIGNAL[UBITS] >> 12;
    // divide group values by CF and find the new maxvalue
    T maxval = 0;
    T cfgroup[B2];
    for (size_t i = 0; i < B2; i++) {
        auto val = magsdiv(group[i], cf);
        maxval = std::max(maxval, val);
        cfgroup[i] = val;
    }

    cf -= 2; // Bias down, 0 and 1 are not used
    auto trung = topbit(maxval | 1); // cf mode rung
    auto cfrung = topbit(cf | 1); // rung for cf-2 value
    // Encode the trung, with or without switch
    // Use the wrong way switch for in-band
    auto cs = CSW[UBITS][(trung - oldrung) & ((1ull << UBITS) - 1)];
    if ((cs >> 12) == 1) // Would be no-switch, use signal instead, it decodes to delta of zero
        cs = SIGNAL[UBITS];
    // When trung is only somewhat larger than cfrung encode cf at same rung as data
    // TODO: The second part of this condition may increase the size in some cases. Why?
    if (trung >= cfrung && trung < (cfrung + UBITS)) {
        acc |= (static_cast<uint64_t>(cs) & 0xffull) << abits;
        abits += cs >> 12;

        if (trung == 0) { // Special encoding for single bit
            // maxval can't be zero, so we don't use the all-zeros flag
            // But we do need to save the CF bit
            acc |= static_cast<uint64_t>(cf) << abits++;
            // And the group bits
            for (int i = 0; i < B2; i++)
                acc |= static_cast<uint64_t>(cfgroup[i]) << abits++;
            // store it directly in the main output stream
            bits.push(acc, abits);
            return; // done
        }

        cfrung = trung; // Encode cf value with trung
        // Push the accumulator and the cf encoding

        // cfrung can't be zero or 63
        assert(cfrung > 0 && cfrung < 63);
        // Use the table version, since cfrung may be 1
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
        bits.push(acc, abits);
    }
    else { // CF needs a different rung than the group, so the change is never 0
        // First, encode trung using code-switch with the change bit cleared
        acc |= (cs & 0xfeull) << abits;
        abits += cs >> 12;

        // Then encode cfrung, using code-switch from trung, 
        // skip the change bit, since rung will always be different
        // cfrung - trung is never 0
        cs = CSW[UBITS][(cfrung - trung) & ((1ull << UBITS) - 1)];
        acc |= (cs & (TBLMASK - 1)) << (abits - 1);
        abits += static_cast<size_t>(cs >> 12) - 1;

        // Push the accumulator and the cf encoding
        // cfrung can't be zero
        // Could use the accumulator and let groupencode deal with last part
        assert(cfrung > 0 && cfrung < 65);
        // Use the table version, since cfrung may be 1

        assert(cf >> cfrung); // Value is in the long group

        if (cfrung > 1) {
            auto p = qb3csztbl(cf ^ (1ull << cfrung), cfrung - 1);
            if (p.first + abits > 64) {
                bits.push(acc, abits);
                acc = abits = 0;
            }
            acc |= p.second << abits;
            abits += p.first;
        }
        else { // single bit, cfrung 0 or 1
            cf -= static_cast<T>(cfrung * 2); // flip the top bit for cfrung 1
            acc |= static_cast<uint64_t>(cf) << abits++;
        }
        bits.push(acc, abits);
    }

    // And the reduced group
    groupencode(cfgroup, maxval, bits);
}

template <typename T = uint8_t>
bool encode_cf(oBits s, const std::vector<T>& image, size_t xsize, size_t ysize, int mb = 1)
{
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    const size_t bands = image.size() / xsize / ysize;
    assert(image.size() == xsize * ysize * bands);
    assert(0 == xsize % B && 0 == ysize % B);

    size_t ssize; // Size of bitstream, if needed
#if defined(HISTOGRAM)
// A histogram of encoded group sizes
    std::map<size_t, size_t> group_sizes;
#endif
    size_t count = 0;

    // Running code length, start with nominal value
    std::vector<size_t> runbits(bands, sizeof(T) * 8 - 1);
    std::vector<T> prev(bands, 0u);      // Previous value, per band
    T group[B2];  // Current 2D group to encode, as array
    size_t offsets[B2];
    uint64_t acc;
    size_t abits = 0;
    for (size_t i = 0; i < B2; i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;

    for (size_t y = 0; y < ysize; y += B) {
        for (size_t x = 0; x < xsize; x += B) {
            size_t loc = (y * xsize + x) * bands; // Top-left pixel address
            for (size_t c = 0; c < bands; c++) { // blocks are band interleaved
                T maxval(0); // Maximum mag-sign value within this group
                auto oldrung = runbits[c];
                { // Collect the block for this band, convert to running delta mag-sign
                    auto prv = prev[c];
                    if (mb != c && mb >= 0 && mb < bands) {
                        for (size_t i = 0; i < B2; i++) {
                            T g = image[loc + c + offsets[i]] - image[loc + mb + offsets[i]];
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
                }

                ssize = s.size();
                const size_t rung = topbit(maxval | 1); // Force at least one bit set
                runbits[c] = rung;

#if defined(_DEBUG)
                if (x == 0 * B && y == 0 * B) {
                    printf("\nLen %04llx", s.size());
                    printf("\nCOMP x %u y %u c %u, rung %u\t", int(x/B), int(y/B), int(c), int(rung));
                    for (int i = 0; i < B2; i++)
                        printf("%u\t", int(group[i]));
                }
#endif

                if (0 == rung) { // only 1s and 0s, rung is -1 or 0
                    // Encode as QB3 group, no point in trying other modes
                    acc = CSW[UBITS][(rung - oldrung) & ((1ull << UBITS) - 1)];
                    abits = acc >> 12;
                    acc &= 0xffull;
                    acc |= static_cast<uint64_t>(maxval) << abits++; // Add the all-zero flag
                    if (0 != maxval)
                        for (size_t i = 0; i < B2; i++)
                            acc |= static_cast<uint64_t>(group[i]) << abits++;
                    s.push(acc, abits);

#if defined(HISTOGRAM)
                    group_sizes[abits]++;
#endif
                    continue;
                }

                // Try the common factor
                auto cf = gcode(group);
                if (cf > 1)
                    cfgenc(s, group, cf, oldrung);
                else
                    groupencode(group, maxval, oldrung, s);

#if defined(HISTOGRAM)
                group_sizes[s.size() - ssize]++;
#endif

            }
        }
    }

#if defined(HISTOGRAM)
    for (auto it : group_sizes)
        printf("%d, %d\n", int(it.first), int(it.second));
#endif
    if (count)
        printf("Count is %d, %f\n", int(count), 
            (count * 100.0) / (double(xsize) * ysize * bands / B2));
    return true;
}


// fast basic encoding
template <typename T = uint8_t>
bool encode_fast(oBits& s, const std::vector<T>& image,
    size_t xsize, size_t ysize, int mb = 1)
{
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    const size_t bands = image.size() / xsize / ysize;
    assert(image.size() == xsize * ysize * bands);
    assert(0 == xsize % B && 0 == ysize % B);

    // Running code length, start with nominal value
    std::vector<size_t> runbits(bands, sizeof(T) * 8 - 1);
    std::vector<T> prev(bands, 0u);      // Previous value, per band
    T group[B2];  // Current 2D group to encode, as array
    size_t offsets[B2];

#if defined(HISTOGRAM)
    // A histogram of encoded group sizes
    size_t ssize = 0;
    std::map<size_t, size_t> group_sizes;
#endif

    size_t count = 0;
    for (size_t i = 0; i < B2; i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;
    for (size_t y = 0; y < ysize; y += B) {
        for (size_t x = 0; x < xsize; x += B) {
            size_t loc = (y * xsize + x) * bands; // Top-left pixel address
            for (size_t c = 0; c < bands; c++) { // blocks are band interleaved

#if defined(HISTOGRAM)
                if (s.size() - ssize)
                    group_sizes[s.size() - ssize]++;
                ssize = s.size();
#endif
                T maxval(0); // Maximum mag-sign value within this group
                { // Collect the block for this band, convert to running delta mag-sign
                    auto prv = prev[c];
                    // Use separate loops to avoid a test inside the loop
                    if (mb != c && mb >= 0 && mb < bands) {
                        for (size_t i = 0; i < B2; i++) {
                            T g = image[loc + c + offsets[i]] - image[loc + mb + offsets[i]];
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
                }
                const size_t rung = topbit(maxval | 1); // Force at least one bit set

                // Encode rung switch using tables, works even with no rung change
                uint64_t acc = CSW[UBITS][(rung - runbits[c]) & ((1ull << UBITS) - 1)];
                size_t abits = acc >> 12;
                acc &= 0xffull; // Strip the size
                runbits[c] = rung;

                if (0 == rung) { // only 1s and 0s, rung is -1 or 0
                    acc |= uint64_t(maxval) << abits++;
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

#if defined(HISTOGRAM)
    for (auto it : group_sizes)
        printf("%d, %d\n", int(it.first), int(it.second));
#endif

    return true;
}

}