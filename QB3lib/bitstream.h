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

Content: Bit streams, in low endian format

*/

#pragma once
#include <cinttypes>
#include <cassert>
#include <type_traits>
#include <limits>

// Input bitstream, doesn't go past size
class iBits {
public:
    iBits(const uint8_t* data, size_t size) : v(data), len(size), bitp(0) {}

    // Single bit fetch
    uint64_t get() {
        if (empty()) return 0; // Don't go past the end
        uint64_t val = static_cast<uint64_t>((v[bitp / 8] >> (bitp % 8)) & 1);
        bitp++;
        return val;
    }

    // Not very efficient for small number of bits
    uint64_t pull(size_t bits = 1) {
        assert(bits && bits <= 64 && !empty());

        uint64_t val = peek() & (~0ull >> (64 - bits));
        advance(bits);
        return val;
    }

    // Advance read position by d bits
    void advance(size_t d) {
        bitp = (bitp + d < len * 8) ? (bitp + d) : (len * 8);
    }

    // Get 64bits without changing the state
    uint64_t peek() const {
        uint64_t val = 0;
        size_t bits = 0;
        if (bitp % 8) { // partial bits
            val = v[bitp / 8] >> (bitp % 8);
            bits = 8 - (bitp % 8);
        }

        // (bitp + bits) is now byte aligned, we need data from 8 more bytes
        for (; bits < 64 && bitp + bits < len * 8; bits += 8)
            val |= static_cast<uint64_t>(v[(bitp + bits) / 8]) << bits;
        return val;
    }

    // informational
    size_t avail() const { return len * 8 - bitp; }
    bool empty() const { return len * 8 <= bitp; }

    size_t position() const {
        return bitp;
    }

private:
    const uint8_t* v;
    const size_t len;

    // next bit to read
    size_t bitp;
};

// Output bitstream, doesn't check the output buffer size
class oBits {
public:
    oBits(uint8_t * data) : v(data), bitp(0) {}

    size_t size_bits() const {
        return bitp;
    }

    // Do not call with val having bits above "nbits" set, the results will be corrupt
    template<typename T>
    void push(T val, size_t nbits) {
        static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
            "Only works with unsigned integral types");
        assert(nbits < 65);
        size_t used = 0;
        if (bitp % 8 != 0) { // Partial byte at the end
            v[bitp / 8] |= static_cast<uint8_t>(val << (bitp % 8));
            used = 8ull - (bitp % 8);
        }
        for (; nbits > used; used += 8)
            v[(bitp + used) / 8] = static_cast<uint8_t>(val >> used);
        bitp += nbits;
    }

private:
    uint8_t *v;

    // bit pointer
    size_t bitp;
};
