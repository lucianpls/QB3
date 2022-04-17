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

Content: Decoder for QB3 compression

Contributors:  Lucian Plesea
*/

#pragma once
#include "bitstream.h"
#include <cinttypes>
#include <utility>
#include <vector>
#include <type_traits>
#include "QB3common.h"

namespace QB3 {

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
// Accumulator should be valid and almost full
// returns false on failure
template<typename T>
static bool gdecode(iBits &s, size_t rung, T * group, uint64_t acc, size_t abits) {
    assert(abits <= 8);
    if (abits)
        acc >>= abits;
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
        return true;
    }
    // Byte decoding is always done with tables
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
        else if (6 > rung) { // Table decode at 3,4 and 5, half of the values fit in accumulator
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
        else { // Last part of table decoding, rungs 6-10 (or 6-7), four values per accumulator
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
    else {     // Large types and high rung Computed decoding, one stream read per value
        if (sizeof(T) < 8 || rung < 32) { // 16 and 32 bits may reuse accumulator at times
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
        else { // May need 65 bits, worst case
            s.advance(abits);
            for (int i = 0; i < B2; i++) {
                auto p = qb3dsz(s.peek(), rung);
                auto ovf = p.first & (p.first >> 6);
                group[i] = static_cast<T>(p.second);
                s.advance(p.first ^ ovf);
                if (ovf) // The next to top bit got dropped
                    group[i] |= s.get() << 62;
            }
        }
    }
    if ((0 == (group[B2 - 1] >> rung)) && (rung > 0)) {
        auto p = step(group, rung);
        if (p < B2)
            group[p] ^= static_cast<T>(1) << rung;
        else if (p == B2)
            return false;
    }
    return true;
}

// integer multiply val(in magsign) by cf(normal)
template<typename T>
static T magsmul(T val, T cf) {
    return magsabs(val) * (cf << 1) - (val & 1);
}

template<typename T>
static bool decode(uint8_t *src, size_t len, T* image, 
    size_t xsize, size_t ysize, size_t bands, size_t *cband)
{
    static_assert(std::is_integral<T>() && std::is_unsigned<T>(), "Only unsigned integer types allowed");
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    iBits s(src, len);
    // vectors as fixed size buffers
    std::vector<T> _prev(bands, 0);
    std::vector<size_t> _runbits(bands, 0);
    auto prev = _prev.data();
    auto runbits = _runbits.data();
    T group[B2];
    size_t offset[B2];
    for (size_t i = 0; i < B2; i++)
        offset[i] = (xsize * ylut[i] + xlut[i]) * bands;
    bool failed(false);
    for (size_t y = 0; (y + B) <= ysize; y += B) {
        for (size_t x = 0; (x + B) <= xsize; x += B) {
            size_t loc = (y * xsize + x) * bands;
            for (int c = 0; c < bands; c++) {
                uint64_t cs(0), abits(1), acc(s.peek());
                if ((acc & 1) != 0) { // Rung change
                    cs = DSW[UBITS][(acc >> 1) & ((1ull << (UBITS + 1)) - 1)];
                    abits = static_cast<size_t>(cs >> 12);
                }
                if (abits == 1 || 0 != (cs & TBLMASK)) { // Normal decoding, not a signal
                    runbits[c] = (runbits[c] + cs) & ((1ull << UBITS) - 1);
                    failed |= !gdecode(s, runbits[c], group, acc, abits);
                }
                else { // signal, cf decoding
                    T cf;
                    size_t cfrung;
                    // The rung switch for the values
                    cs = DSW[UBITS][(acc >> (abits + 1)) & ((1ull << (UBITS + 1)) - 1)];
                    auto rung = (runbits[c] + cs) & ((1ull << UBITS) - 1);
                    failed |= (rung == 63); // can't be 63 since CF encoding looses at least one rung
                    if ((acc >> abits) & 1) { // same rung for cf and values
                        abits += static_cast<size_t>(cs >> 12);
                        cfrung = rung;
                    }
                    else { // cfrung is separate, it doesn't include the code switch
                        abits += static_cast<size_t>(cs >> 12);
                        cs = DSW[UBITS][(acc >> abits) & ((1ull << (UBITS + 1)) - 1)];
                        abits += static_cast<size_t>(cs >> 12) - 1;
                        cfrung = (rung + cs) & ((1ull << UBITS) - 1);
                        failed |= (rung == cfrung);
                    }
                    if (0 == (rung | cfrung)) { // single bit for everything, decode here
                        acc >>= abits;
                        runbits[c] = 1ull + (acc & 1);
                        if (acc & 1)
                            for (int i = 0; i < B2; i++) {
                                acc >>= 1;
                                group[i] = static_cast<T>((acc & 1) * 0b101);
                            }
                        else
                            for (int i = 0; i < B2; i++) {
                                acc >>= 1;
                                group[i] = static_cast<T>((acc & 1) * 0b11);
                            }
                        s.advance(abits + B2 + 1);
                    }
                    else {
                        if (cfrung + abits > 62) {
                            s.advance(abits);
                            acc = s.peek();
                            abits = 0;
                        }
                        // There is no overflow possible here, cfrung is < 63 and > 0
                        if (cfrung == rung) { // standard encoding
                            auto p = qb3dsztbl(acc >> abits, cfrung);
                            cf = static_cast<T>(p.second);
                            abits += p.first;
                        }
                        else { // cfrung is different, thus the encoded value is always in long range
                            if (cfrung > 1) {
                                auto p = qb3dsztbl(acc >> abits, cfrung - 1);
                                cf = static_cast<T>(p.second + (1ull << cfrung));
                                abits += p.first;
                            }
                            else // single bit
                                cf = static_cast<T>(((acc >> abits++) & 1) + cfrung * 2);
                        }
                        if (abits > 8) {
                            s.advance(abits);
                            acc = s.peek();
                            abits = 0;
                        }
                        failed |= !gdecode(s, rung, group, acc, abits);
                        // Multiply with CF and get the maxval for the actual group rung
                        T maxval = 0;
                        for (int i = 0; i < B2; i++) {
                            auto v = magsmul(group[i], T(cf + 2));
                            maxval = std::max(maxval, v);
                            group[i] = v;
                        }
                        failed |= (2 > maxval);
                        runbits[c] = topbit(maxval | 1); // Still, don't call topbit with 0
                    }
                }
                auto prv = prev[c];
                for (int i = 0; i < B2; i++)
                    image[loc + c + offset[i]] = prv += smag(group[i]);
                prev[c] = prv;
            }
            for (int c = 0; c < bands; c++)
                if (cband[c] != c)
                    for (size_t i = 0; i < B2; i++)
                        image[loc + c + offset[i]] += image[loc + cband[c] + offset[i]];
            if (failed) break;
        }
        if (failed) break;
    }
    return failed;
}
} // Namespace