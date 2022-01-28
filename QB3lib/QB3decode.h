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

Content: Decoder for QB3 compression

Contributors:  Lucian Plesea
*/

#pragma once
#include "bitstream.h"
#include <cinttypes>
#include <utility>
#include <vector>

namespace QB3 {
#include "QB3common.h"

// Computed decode, does not work for rung 0 or 1
static std::pair<size_t, uint64_t> qb3dsz(uint64_t val, size_t rung) {
    assert(rung > 1);
    uint64_t ntop = (~(val >> (rung - 1))) & 1;
    uint64_t rmsk = (1ull << rung) - 1;
    if (1 & ~ntop)
        return std::make_pair(rung, val & (rmsk >> 1));
    uint64_t nnxt = (~(val >> (rung - 2))) & 1;
    return std::make_pair(rung + 1 + nnxt,
        (((1 & ~nnxt) * ~0ull) & (((val << 1) & rmsk) | ((val >> rung) & 1)))
        + (((1 & nnxt) * ~0ull) & ((rmsk + 1) + ((val & (rmsk >> 1)) << 2) + ((val >> rung) & 0b11))));
}

// Decode using tables when possible, works for rungs 1+
static std::pair<size_t, uint64_t> qb3dsztbl(uint64_t val, size_t rung) {
    assert(rung);
    if ((sizeof(DRG) / sizeof(*DRG)) > rung) {
        auto code = DRG[rung][val & ((1ull << (rung + 2)) - 1) ];
        return std::make_pair<size_t, uint64_t>(code >> 12, code & TBLMASK);
    }
    return qb3dsz(val, rung);
}

// Decode a B2 sized group of QB3 values from s and acc
// Accumulator should be valid and almost full
template<typename T>
void gdecode(iBits &s, size_t rung, T group[B2], uint64_t acc, size_t abits) {
    assert(abits <= 8);
    if (0 == rung) { // single bits, special case, need at least 17bits in accumulator
        if (0 != ((acc >> abits++) & 1)) {
            for (int i = 0; i < B2; i++)
                group[i] = static_cast<T>((acc >> abits++) & 1);
        }
        else {
            for (int i = 0; i < B2; i++)
                group[i] = static_cast<T>(0);
        }
        s.advance(abits);
    }
    else if (rung < 6) { // Table decode, half of the values fit in accumulator
        const auto drg = DRG[rung];
        const auto m = (1ull << (rung + 2)) - 1;
        for (size_t i = 0; i < B2 / 2; i++) {
            auto v = drg[(acc >> abits) & m];
            abits += v >> 12;
            group[i] = static_cast<T>(v & TBLMASK);
        }
        // Skip the peek if we have enough bits in accumulator
        // At rung 3, only for abits = 24 is possible to skip the load
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
    }
    // Last part of table decoding, use the accumulator for every 4 values
    else if (rung < (sizeof(DRG) / sizeof(*DRG))) {
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
    }
    // Computed decoding, one stream read per value
    else if (sizeof(T) != 1) {
        s.advance(abits);
        if (sizeof(T) != 8) {
            for (int i = 0; i < B2; i++) {
                auto p = qb3dsz(s.peek(), rung);
                s.advance(p.first);
                group[i] = static_cast<T>(p.second);
            }
        }
        else { // Only for 64bit data
            if (63 != rung) {
                for (int i = 0; i < B2; i++) {
                    auto p = qb3dsz(s.peek(), rung);
                    s.advance(p.first);
                    group[i] = static_cast<T>(p.second);
                }
            }
            else { // Might require more than one read
                for (int i = 0; i < B2; i++) {
                    auto p = qb3dsz(s.peek(), rung);
                    auto ovf = p.first & (p.first >> 6);
                    group[i] = static_cast<T>(p.second);
                    s.advance(p.first - ovf);
                    if (ovf)
                        group[i] |= s.get() << 1;
                }
            }
        }
    }
    // Undo the step shift, MSB of last value has to be zero
    if ((0 == (group[B2 - 1] >> rung)) & (rung > 0)) {
        auto p = step(group, rung);
        assert(p != B2); // Can't occur, could be a signal
        if (p < B2)
            group[p] ^= static_cast<T>(1) << rung;
    }
}

// integer multiply val(in magsign) by cf(normal)
template<typename T>
static inline T magsmul(T val, T cf) {
    return magsabs(val) * (cf << 1) - (val & 1);
}

template<typename T>
bool decode(uint8_t *src, size_t len, T* image,
    size_t xsize, size_t ysize, size_t bands, size_t *cband)
{
    bool failure(false);
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    iBits s(src, len);
    std::vector<T> prev(bands, 0);
    T group[B2];
    std::vector<size_t> runbits(bands, sizeof(T) * 8 - 1);
    std::vector<size_t> offsets(B2);
    for (size_t i = 0; i < B2; i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;

    for (size_t y = 0; (y + B) <= ysize; y += B) {
        if (failure) 
            break;
        for (size_t x = 0; (x + B) <= xsize; x += B) {
            if (failure) 
                break;
            size_t loc = (y * xsize + x) * bands;
            for (int c = 0; c < bands; c++) {
                uint64_t acc = s.peek();
                size_t abits = 1;
                size_t cs = 0;
                if ((acc & 1) != 0) { // Rung change
                    cs = DSW[UBITS][(acc >> 1) & ((1ull << (UBITS + 1)) - 1)];
                    abits = static_cast<size_t>(cs >> 12);
                }
                if (abits == 1 || 0 != (cs & TBLMASK)) { // Normal decoding, not a signal
                    auto rung = (runbits[c] + cs) & ((1ull << UBITS) - 1);
                    runbits[c] = rung;
                    gdecode(s, rung, group, acc, abits);
                }
                else { // signal, cf decoding
                    T cf;
                    size_t cfrung;
                    // The rung switch for the values
                    cs = DSW[UBITS][(acc >> (abits + 1)) & ((1ull << (UBITS + 1)) - 1)];
                    auto rung = (runbits[c] + cs) & ((1ull << UBITS) - 1);
                    failure |= (rung == 63);
                    // assert(!failure); // can't be 63 since CF encoding looses at least one rung

                    if ((acc >> abits) & 1) { // same rung for cf and values
                        abits += static_cast<size_t>(cs >> 12);
                        cfrung = rung;
                    }
                    else { // cfrung is separate, it doesn't include the code switch
                        abits += static_cast<size_t>(cs >> 12);
                        cs = DSW[UBITS][(acc >> abits) & ((1ull << (UBITS + 1)) - 1)];
                        abits += static_cast<size_t>(cs >> 12) - 1;
                        cfrung = (rung + cs) & ((1ull << UBITS) - 1);
                        // cfrung has to be different than rung here, either larger or much smaller
                        failure |= (rung == cfrung);
                        // assert(!failure); // Bad encoding?
                    }

                    if (0 == (rung | cfrung)) { // single bit encoding for everything
                        cf = static_cast<T>((acc >> abits++) & 1);
                        // cf is 0 or 1, which means 2 or 3 encoded as mags
                        // while the value can only be 0 or -1, encoded as mags 
                        static const uint8_t tbl[] = { 0, 0b11, 0, 0b101 };
                        for (int i = 0; i < B2; i++)
                            group[i] = static_cast<T>(tbl[cf * 2ull + ((acc >> abits++) & 1)]);

                        // actual rung is 1 or 2 respectively
                        runbits[c] = 1ull + cf;
                        s.advance(abits);
                    }
                    else {
                        // check and refill accumulator before reading cf
                        if (cfrung + 2 + abits > 64) {
                            s.advance(abits);
                            abits = 0;
                            acc = s.peek();
                        }

                        // There is no overflow possible here, trung is < 64 and > 0
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
                            else { // single bit
                                cf = static_cast<T>(((acc >> abits++) & 1) + cfrung * 2);
                            }
                        }
                        s.advance(abits);
                        // Read the group, refresh the accumulator
                        gdecode(s, rung, group, s.peek(), 0);
                        // Multiply with CF and get the maxval for the actual group rung
                        cf += 2;
                        T maxval = 0;
                        for (int i = 0; i < B2; i++) {
                            auto v = magsmul(group[i], cf);
                            maxval = std::max(maxval, v);
                            group[i] = v;
                        }
                        failure |= (2 > maxval);
                        runbits[c] = topbit(maxval | 1); // Still, don't call topbit with 0
                    }
                }
                auto prv = prev[c];
                for (int i = 0; i < B2; i++)
                    image[loc + c + offsets[i]] = prv += smag(group[i]);
                prev[c] = prv;
            }
            for (int c = 0; c < bands; c++)
                if (cband[c] != c)
                    for (size_t i = 0; i < B2; i++)
                        image[loc + c + offsets[i]] += image[loc + cband[c] + offsets[i]];
        }
    }
    return 0;
}
} // Namespace