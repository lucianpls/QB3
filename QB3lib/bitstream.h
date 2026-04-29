/*
Content: Bit stream IO, in low endian format

Copyright 2020-2026 Esri
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
    oBits(uint8_t * data) : acc(0), v(data), bitp(0) {}

    // Number of bits written
    size_t position() const { return bitp; }

    // Rewind to a position before the current one
    size_t rewind(size_t pos = 0) {
        if (pos <= bitp) { // Only backward
            bitp = pos;
            acc = 0;
            if (bitp & 63) {
                // Assumes that the 64 bit read does not overflow output buffer, caller's responsibility
                acc = reinterpret_cast<const uint64_t*>(v)[pos / 64];
                acc &= ~0ull >> (64 - (pos & 63));
            }
        }
        return bitp; // curent position
    }

    // Do not call with val having bits above "nbits" set
    template<typename T>
    void push(T val, size_t nbits) {
        static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
            "Only works with unsigned integral types");
        assert(nbits < 65);
        auto acc_bits = bitp & 63; // Number of bits in the accumulator
        // Add the new bits to the accumulator
        acc |= static_cast<uint64_t>(val) << acc_bits;
        if (acc_bits + nbits >= 64) {
            // Flush the accumulator which is now full
            reinterpret_cast<uint64_t*>(v)[bitp / 64] = acc;
            // Start a new accumulator with the remaining bits, if any
            acc = acc_bits ? val >> (64 - acc_bits) : 0;
        }
        bitp += nbits;
    }

    template<typename T> void push(std::pair<size_t, T> p) { push(p.second, p.first); }

    // Append content from other output bitstream, which should be flushed
    oBits& operator+=(oBits& other) {
        other.flush();
        // Copy the full 64bit words
        auto const pv = reinterpret_cast<const uint64_t*>(other.v);
        for (int i = 0; i < other.bitp / 64; i++)
            push(pv[i], 64);

        // Copy the remaining bits, if any
        auto acc_bits = other.bitp & 63; // Number of bits in the accumulator
        if (acc_bits != 0)
            push(pv[other.bitp / 64] & (~0ull >> (64 - acc_bits)), acc_bits);

        return *this;
    }

    // Flush stream, making sure that bytes are writtent to the buffer
    void flush() {
        if (bitp & 63)
            reinterpret_cast<uint64_t*>(v)[bitp / 64] = acc;
    }

    // Flush and round position to byte boundary
    size_t tobyte() {
        flush();
        bitp = (bitp + 7) & ~0x7;
        if (0 == (bitp & 63)) // Clear the accumulator if at 64 bit boundary
            acc = 0;
        return bitp >> 3; // In bytes
    }

private:
    uint64_t acc; // Accumulator for bits not yet written to the output
    uint8_t *v;
    size_t bitp; // write position, includes up to 63 bits in the accumulator
};
