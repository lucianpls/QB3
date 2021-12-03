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
#include <cinttypes>
#include <vector>
#include "bitstream.h"

namespace QB3 {
#include "QB3common.h"

// Reverse mag-sign and delta
template<typename T>
static T undsign(T* v, T pred) {
    for (size_t i = 0; i < B2; i++)
        v[i] = pred += smag(v[i]);
    return pred;
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
                    if (0 == (cs & 0xff)) { // Encoding type signal detection, long-no-switch
//                        printf("Signal\n");
                    }
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

                // Computed decoding, single stream read
                if (sizeof(T) > 1) {
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
                }

            GROUP_DONE:
                // Undo the step shift, MSB of last value has to be zero
                if ((0 == (group[B2 - 1] >> rung)) & (rung > 0)) {
                    auto p = step(group, rung);
                    assert(p != B2); // Can't occur, could be a signal
                    if (p < B2)
                        group[p] ^= static_cast<T>(1) << rung;
                }

                auto prv = prev[c];
                for (size_t i = 0; i < B2; i++)
                    image[loc + c + offsets[i]] = prv += smag(group[i]);
                prev[c] = prv;
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