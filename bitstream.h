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
#include <cassert>

// TODO: Write these with c arrays instead of vector

class iBits {
public:
    iBits(const std::vector<uint8_t>& data) : v(data), bitp(0) {}

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
        bitp = (bitp + d < v.size() * 8) ? (bitp + d) : (v.size() * 8);
    }

    // Get 64bits without changing the state
    uint64_t peek() const {
        assert(!empty()); // Fine to call past the end, but at least one bit should be available

        uint64_t val = 0;
        size_t bits = 0;
        if (bitp % 8) { // partial bits
            val = v[bitp / 8] >> (bitp % 8);
            bits = 8 - (bitp % 8);
        }

        // (bitp + bits) is now byte aligned, we need data from 8 more bytes
        for (; bits < 64 && bitp + bits < v.size() * 8; bits += 8)
            val |= static_cast<uint64_t>(v[(bitp + bits) / 8]) << bits;
        return val;
    }

    // informational
    size_t avail() const { return v.size() * 8 - bitp; }
    bool empty() const { return v.size() * 8 <= bitp; }

private:
    const std::vector<uint8_t>& v;

    // next bit to read
    size_t bitp;
};

class oBits {
public:
    oBits(std::vector<uint8_t>& data) : v(data), bitp(0) {}
    oBits(std::vector<uint8_t>& data, size_t bitsize) : v(data), bitp(0) {
        assert((bitsize + 7) / 8 == data.size());
        setSize(bitsize);
    }

    size_t size() const {
        return v.size() * 8  - ((8 - bitp) & 7);
    }

    // Can only shrink the stream
    size_t setSize(size_t newsize) {
        // if the new size is identical, it just clears the last few bits
        if (newsize <= size()) {
            v.resize((newsize + 7) / 8);
            bitp = newsize & 7;
            if (bitp) // Clear the unused bits in last byte
                v.back() &= 0xff >> (8 - bitp);
        }
        return size();
    }

    // Do not call with val having bits above "nbits" set, the results will be corrupt
    template<typename T>
    void push(T val, size_t nbits) {
        static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
            "Only works with unsigned integral types");
        assert(nbits && nbits < 65);
        size_t used = 0;
        if (bitp != 0) { // Partial byte
            v.back() |= static_cast<uint8_t>(val << bitp);
            used = 8ull - bitp;
        }
        for (;nbits > used; used+=8)
            v.push_back(static_cast<uint8_t>(val >> used));
        bitp = (bitp + nbits) & 7ull;
    }

    // Append
    oBits& operator+=(const oBits& other) {
        assert(v != other.v);
        v.reserve(v.size() + other.v.size());
        if (bitp) { // unaligned
            auto os = other.size();
            iBits ins(other.v);
            while (os >= 64) {
                auto acc = ins.peek();
                v.back() |= acc << bitp; // leftover bits
                acc >>= 8 - bitp; // byte align
                v.resize(v.size() + 8);
                memcpy(&v[v.size() - 8], &acc, 8);
                ins.advance(64);
                os -= 64;
            }

            // Last part, under 64 bits
            if (os) {
                auto acc = ins.peek(); // Last few bits
                v.back() |= acc << bitp; // align
                acc >>= 8 - bitp;
                os -= std::min(os, 8 - bitp); // don't go under 0
                while (os > 0) {
                    v.push_back(static_cast<uint8_t>(acc));
                    acc >>= 8;
                    os -= std::min(os, size_t(8));
                }
            }
            bitp = (bitp + other.size()) & 7ull;
        }
        else { // This stream is all byte aligned, just copy the bytes
            v.resize(v.size() + other.v.size());
            memcpy(&v[v.size() - other.v.size()], other.v.data(), other.v.size());
            bitp = other.bitp;
        }
        setSize(size()); // clears the last few bits, if needed
        return *this;
    }

private:
    std::vector<uint8_t>& v;

    // bit index, within a byte, for next write
    size_t bitp;

};
