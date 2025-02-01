/*
Content: Bit stream IO, in low endian format

Copyright 2020-2025 Esri
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

#include <cinttypes>
#include <cassert>
#include <type_traits>
#include <limits>
#include <utility>

// Input bitstream, doesn't go past size
class iBits {
public:
    iBits(const uint8_t* data, size_t size) : v(data), len(size * 8), bitp(0) {}

    // informational
    size_t avail() const { return len - bitp; }
    bool empty() const { return avail() == 0; }
    // read bit position
    size_t position() const { return bitp; }

    // Advance read position by d bits
    void advance(size_t d) { bitp = (bitp + d < len) ? (bitp + d) : len; }

    // Get 64bits without changing the state
    uint64_t peek() const {
        if (avail() >= 64)
            return (v[bitp / 8] >> (bitp % 8)) |
            (*reinterpret_cast<const uint64_t*>(v + ((bitp + 7) / 8)) << ((8 - bitp) % 8));
        if (empty())
            return 0;
        uint64_t val = v[bitp / 8] >> (bitp % 8);
        // (bitp + bits) is byte aligned, we need data from 7 or 8 more bytes
        for (size_t bits = 8 - (bitp % 8); bits < 64 && bitp + bits < len; bits += 8)
            val |= static_cast<uint64_t>(v[(bitp + bits) / 8]) << bits;
        return val;
    }

    // Not very efficient for small number of bits
    uint64_t pull(size_t bits = 1) {
        uint64_t val = peek() & (~0ull >> (64 - bits));
        advance(bits);
        return val;
    }

private:
    const uint8_t* v;
    const size_t len; // in bits, multiple of 8
    size_t bitp; // current bit position
};

// Output bitstream, assumes enough space
class oBits {
public:
    oBits(uint8_t * data) : v(data), bitp(0) {}

    // Number of bits written
    size_t position() const { return bitp; }

    // Rewind to a position before the current one
    size_t rewind(size_t pos = 0) {
        if (pos < bitp) { // Only backward
            bitp = pos;
            if (pos & 7) // clear last byte partial bits
                v[pos / 8] &= 0xff >> (8 - (pos & 7));
        }
        return bitp; // final position
    }

    // Do not call with val having bits above "nbits" set
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

    template<typename T> void push(std::pair<size_t, T> p) { push(p.second, p.first); }

    // Append content from other output bitstream
    oBits& operator+=(const oBits& other) {
        auto len = other.bitp;
        for (auto pv = reinterpret_cast<uint64_t*>(other.v); len >= 64; len -= 64, pv++)
            push(*pv, 64);
        // bits at the end
        if (len) {
            uint64_t acc = 0;
            auto pv = other.v + (other.bitp / 64) * 8;
            for (size_t i = 0; i * 8 < len; i++)
                acc |= static_cast<uint64_t>(pv[i]) << (i * 8);
            push(acc, len);
        }
        return *this;
    }

    // Round position to byte boundary
    size_t tobyte() {
        bitp = (bitp + 7) & ~0x7;
        return bitp >> 3; // In bytes
    }

private:
    uint8_t *v;
    size_t bitp; // write position
};
