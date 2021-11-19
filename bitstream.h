#pragma once
#include <vector>

// bitstream in low endian format, up to 64 bits at a time
struct Bitstream {
    std::vector<uint8_t>& v;
    size_t bitp; // Next bit for input, absolute number
    Bitstream(std::vector<uint8_t>& data) : v(data), bitp(0) {};

    void clear() {
        bitp = 0;
        v.clear();
    }

    // Do not call with val having bits above "bits" set, the results will be corrupt
    template<typename T>
    void push(T val, size_t bits) {
        static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
            "Only works for unsigned integral types");

        for (; bits;) {
            if (bitp)
                v.back() |= static_cast<uint8_t>(val << bitp);
            else
                v.push_back(static_cast<uint8_t>(val));
            size_t used = std::min(8 - bitp, bits);
            bits -= used;
            bitp = (bitp + used) & 7;
            val >>= used;
        }
    }

    template<typename T = uint64_t>
    bool pull(T& val, size_t bits = 1) {
        static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
            "Only works for unsigned integral types");
        val = 0;
        size_t accsz = 0; // bits in accumulator
        while (bits && (bitp < v.size() * 8)) {
            size_t used = std::min(8 - bitp % 8, bits);
            val |= static_cast<T>((v[bitp / 8] >> (bitp % 8)) & mask[used]) << accsz;
            bits -= used;
            bitp += used;
            accsz += used;
        }
        return bitp <= v.size() * 8;
    }

    // Get 64bits without changing the state of the stream
    uint64_t peek() const {
        uint64_t val = 0;
        size_t bits = 0;
        if (bitp % 8) { // partial bits
            val = v[bitp / 8] >> (bitp % 8);
            bits = 8 - (bitp % 8);
        }

        // Single test, works most of the time
        //if (bitp + bits + 64 <= v.size() * 8) {
        //    for (int i = 0; i < 8; i++, bits += 8)
        //        val |= static_cast<uint64_t>(v[(bitp + bits) / 8]) << bits;
        //    return val;
        //}

        // (bitp + bits) is now byte aligned, we need data from 8 more bytes
        for (; bits < 64 && bitp + bits < v.size() * 8; bits += 8)
            val |= static_cast<uint64_t>(v[(bitp + bits) / 8]) << bits;
        return val;
    }

    size_t bitsize() const {
        return v.size() * 8 + bitp;
    }

    // Advance bitp if doing so won't go out of bounds
    void advance(size_t d) {
        if (bitp + d < v.size() * 8)
            bitp += d;
    }

    // Single bit fetch
    uint64_t get() {
        if (bitp >= 8 * v.size()) return 0; // Don't go past the end
        uint64_t val = static_cast<uint64_t>((v[bitp / 8] >> (bitp % 8)) & 1);
        bitp++;
        return val;
    }

    static const uint8_t mask[9];
};
