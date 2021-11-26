#pragma once
#include <vector>
#include <cinttypes>

class iBits {
public:

    iBits(std::vector<uint8_t>& data) : v(data), bitp(0) {}

    // Single bit fetch
    uint64_t get() {
        if (bitp >= 8 * v.size()) return 0; // Don't go past the end
        uint64_t val = static_cast<uint64_t>((v[bitp / 8] >> (bitp % 8)) & 1);
        bitp++;
        return val;
    }

    template<typename T = uint64_t>
    bool pull(T& val, size_t bits = 1) {
        static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
            "Only works for unsigned integral types");
        val = 0;
        size_t accsz = 0; // bits in accumulator
        while (bits && (bitp < v.size() * 8)) {
            size_t used = std::min(8 - bitp % 8, bits); // 1 to 8
            val |= static_cast<T>((v[bitp / 8] >> (bitp % 8)) & (0xff >> (8 - used))) << accsz;
            bits -= used;
            bitp += used;
            accsz += used;
        }
        return bitp <= v.size() * 8;
    }

    // Advance read position by d bits
    void advance(size_t d) {
        if (bitp + d < v.size() * 8)
            bitp += d;
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
        for (; bits < 64 && bitp + bits < v.size() * 8; bits += 8)
            val |= static_cast<uint64_t>(v[(bitp + bits) / 8]) << bits;
        return val;
    }


private:
    std::vector<uint8_t>& v;

    // next bit to read
    size_t bitp;
};

class oBits {
public:
    oBits(std::vector<uint8_t>& data) : v(data), bitp(0) {}

    size_t size() const {
        return v.size() * 8  - ((8 - bitp) & 7);
    }

    // Do not call with val having bits above "bits" set, the results will be corrupt
    template<typename T>
    void push(T val, size_t bits) {
        static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
            "Only works with unsigned integral types");

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

private:
    std::vector<uint8_t>& v;

    // bit index, within a byte, for next write
    size_t bitp;

};
