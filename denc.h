#pragma once
#include <vector>
#include <cinttypes>
#include <limits>
#include <functional>

namespace SiBi {
// Masks, from 0 to 64 bits
#define M(v) (~0ull >> (64 - (v)))
// A row of 8 masks, starting with mask(n)
#define R(n) M(n), M(n + 1), M(n + 2), M(n + 3), M(n + 4), M(n + 5), M(n + 6), M(n + 7)
    const uint64_t mask[] = {0, R(1), R(9), R(17), R(25), R(33), R(41), R(49), R(57)};
#undef R
#undef M

// rank of top set bit
static size_t ilogb(uint64_t val) {
    for (int i = 1; i < 65; i++)
        if (val <= mask[i])
            return i - 1;
    return ~0; // not reached
}

// Greater common denominator
template<typename T>
T gcd(const std::vector<T>& vals) {
    if (vals.empty())
        return 0;
    auto v(vals); // Work on a copy
    auto vb(v.begin()), ve(v.end());
    for (;;) {
        sort(vb, ve);
        while (!*vb)
            if (++vb == ve)
                return 0;
        auto m(*vb);
        if (vb + 1 == ve)
            return m;
        for (auto vi(vb + 1); vi < ve; vi++)
            *vi %= m;
    }
    // Not reached
    return 0;
}

// Convert from mag-sign to absolute
template<typename T>
inline T revs(T val) {
    return (val >> 1) + (val & 1);
}

// return greatest common factor
// T is always unsigned
template<typename T>
T gcode(const std::vector<T>& vals) {

    // heap of absolute values
    std::vector<T> v;
    v.reserve(vals.size());
    for (auto val : vals) {
        // ignore the zeros
        if (val == 0)
            continue;
        // if a value is 1 or -1, the only common factor will be 1
        if (val < 3)
            return 1;
        v.push_back(revs(val));
        push_heap(v.begin(), v.end(), std::greater<T>());
    }

    if (v.empty() || v.front() < 3)
        return 1;

    do {
        pop_heap(v.begin(), v.end(), std::greater<T>());
        const T m(v.back()); // never less than 2
        v.pop_back();
        for (auto& t : v) t %= m;
        v.push_back(m); // v is never empty
        make_heap(v.begin(), v.end(), std::greater<T>());
        while (v.size() && v.front() == 0) {
            pop_heap(v.begin(), v.end(), std::greater<T>());
            v.pop_back();
        }
        if (1 == v.front())
            return 1;
    } while (v.size() > 1);
    return v.front();
}

//
// Delta and sign reorder
// All these templates work only for T unsigned, integer, 2s complement types
// Even though T is unsigned, it is considered signed, that's the whole point!
//


// Encode integers as absolute magnitude and sign, so the bit 0 is the sign.
// This encoding has the top bits always zero, regardless of sign
// To keep the range exactly the same as two's complement, the magnitude of negative values is biased down by one (no negative zero)


// These alternates use a conditional assignment that can't be predicted well
// Change to mag-sign
template<typename T>
static T mags_alt(T v) {
    return (v > (std::numeric_limits<T>::max() >> 1)) ? ~(v << 1) : (v << 1);
}

// Undo mag-sign
template<typename T>
static T smag_alt(T v) {
    return (v & 1) ? ~(v >> 1) : (v >> 1);
}

// Change to mag-sign without conditionals, faster
template<typename T>
static T mags(T v) {
    return (std::numeric_limits<T>::max() * (v >> (8 * sizeof(T) - 1))) ^ (v << 1);
}

// Undo mag-sign without conditionals, faster
template<typename T>
static T smag(T v) {
    return (std::numeric_limits<T>::max() * (v & 1)) ^ (v >> 1);
}

// Convert a sequence to mag-sign delta
template<typename T>
static T dsign(std::vector<T>& v, T pred) {
    static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
        "Only works for unsigned integral types");
    for (auto& it : v) {
        pred += it -= pred; // or std::swap(it, pred) ; it = pred - it;
        it = mags(it);
    }
    return pred;
}

// Reverse mag-sign and delta
template<typename T>
static T undsign(std::vector<T>& v, T pred) {
    for (auto& it : v)
        it = pred += smag(it);
    return pred;
}

// Traversal order tables, first for powers of two
static const uint8_t xp2[64] = {
    0, 1, 0, 1, 2, 3, 2, 3,
    0, 1, 0, 1, 2, 3, 2, 3,
    4, 5, 4, 5, 6, 7, 6, 7,
    4, 5, 4, 5, 6, 7, 6, 7,
    0, 1, 0, 1, 2, 3, 2, 3,
    0, 1, 0, 1, 2, 3, 2, 3,
    4, 5, 4, 5, 6, 7, 6, 7,
    4, 5, 4, 5, 6, 7, 6, 7
};

static const uint8_t yp2[64] = {
    0, 0, 1, 1, 0, 0, 1, 1,
    2, 2, 3, 3, 2, 2, 3, 3,
    0, 0, 1, 1, 0, 0, 1, 1,
    2, 2, 3, 3, 2, 2, 3, 3,
    4, 4, 5, 5, 4, 4, 5, 5,
    6, 6, 7, 7, 6, 6, 7, 7,
    4, 4, 5, 5, 4, 4, 5, 5,
    6, 6, 7, 7, 6, 6, 7, 7
};

static const uint8_t x3[9] = {
    0, 1, 0, 1, 2, 2, 0, 1, 2
};
static const uint8_t y3[9] = {
    0, 0, 1, 1, 0, 1, 2, 2, 2
};

static const uint8_t x5[25] = {
    0, 1, 0, 1, 2, 3, 2, 3,
    0, 1, 0, 1, 2, 3, 2, 3,
    4, 4, 4, 4,
    0, 1, 2, 3, 4
};

static const uint8_t y5[25] = {
    0, 0, 1, 1, 0, 0, 1, 1,
    2, 2, 3, 3, 2, 2, 3, 3,
    0, 1, 2, 3,
    4, 4, 4, 4, 4
};

static const uint8_t x6[36] = {
    0, 1, 0, 1, 2, 3, 2, 3,
    0, 1, 0, 1, 2, 3, 2, 3,
    4, 5, 4, 5, 4, 5, 4, 5,
    0, 1, 0, 1, 2, 3, 2, 3,
    4, 5, 4, 5
};

static const uint8_t y6[36] = {
    0, 0, 1, 1, 0, 0, 1, 1,
    2, 2, 3, 3, 2, 2, 3, 3,
    0, 0, 1, 1, 2, 2, 3, 3,
    4, 4, 5, 5, 4, 4, 5, 5,
    4, 4, 5, 5
};

static const uint8_t x7[49] = {
    0, 1, 0, 1, 2, 3, 2, 3,
    0, 1, 0, 1, 2, 3, 2, 3,
    4, 5, 4, 5, 6, 6,
    4, 5, 4, 5, 6, 6,
    0, 1, 0, 1, 2, 3, 2, 3,
    0, 1, 2, 3,
    4, 5, 4, 5, 6, 6,
    4, 5, 6,
};

static const uint8_t y7[49] = {
    0, 0, 1, 1, 0, 0, 1, 1,
    2, 2, 3, 3, 2, 2, 3, 3,
    0, 0, 1, 1, 0, 1,
    2, 2, 3, 3, 2, 3,
    4, 4, 5, 5, 4, 4, 5, 5,
    6, 6, 6, 6,
    4, 4, 5, 5, 4, 5,
    6, 6, 6
};

static const uint8_t* xx[9] = { xp2, xp2, xp2, x3, xp2, x5, x6, x7, xp2 };
static const uint8_t* yy[9] = { yp2, yp2, yp2, y3, yp2, y5, y6, y7, yp2 };

// Block size should be 8, for noisy images 4 is better
// 2 generates too much overhead
// 16 might work for slow varying inputs, 
// which would need the lookup tables extended
// Works for non-power of two blocks, using custom tables
// Encoding with three codeword lenghts
// Does not need the maxvalue to be encoded
// only the reference number of bits
template <typename T = uint8_t>
std::vector<uint8_t> sincode(const std::vector<T>& image,
    size_t xsize, size_t ysize, size_t bsize, int mb = 1)
{
    std::vector<uint8_t> result;
    Bitstream s(result);
    const uint8_t* xlut = xx[bsize];
    const uint8_t* ylut = yy[bsize];
    const size_t bands = image.size() / xsize / ysize;
    // Nominal bit length
    const constexpr uint8_t ubits = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    std::vector<size_t> runbits(bands, sizeof(T)*8 - 1); // Running code length, start with nominal value
    std::vector<T> prev(bands, 0u);      // Previous value per band
    std::vector<T> group(bsize * bsize); // Current 2D group to encode, as vector

    std::vector<size_t> offsets(group.size());
    for (size_t i = 0; i < offsets.size(); i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;

    uint64_t saved(0); // bits saved
    uint64_t saved_count(0); // blocks with bits saved

    for (size_t y = 0; (y + bsize) <= ysize; y += bsize) {
        for (size_t x = 0; (x + bsize) <= xsize; x += bsize) {
            size_t loc = (y * xsize + x) * bands; // Top-left pixel address
            for (size_t c = 0; c < bands; c++) { // blocks are band interleaved
                // Collect the block for this band
                for (size_t i = 0; i < group.size(); i++) {
                    group[i] = image[loc + c + offsets[i]];
                    // Subtract the main band values
                    if (mb >= 0 && mb != c)
                        group[i] -= image[loc + mb + offsets[i]];
                }

                // Delta with low sign group encode
                prev[c] = dsign(group, prev[c]);
                uint64_t maxval = *max_element(group.begin(), group.end());
                if (maxval < 2) {
                    // Special encoding, 000 0 for all zeros, or 000 1 followed by bit vector for 0 and 1
                    if (runbits[c] == 0)
                        s.push(maxval << 1, 2); // one zero bit prefix, same as before and the all zero flag
                    else {
                        s.push((maxval << (ubits + 1)) + 1, ubits + 2); // one 1 bit, then the 0 ubit len, then the zero flag
                        runbits[c] = 0;
                    }
                    if (1 == maxval) {
                        uint64_t val = 0;
                        for (auto it = group.crbegin(); it != group.crend(); it++)
                            val = (val << 1) + *it;
                        s.push(val, group.size());
                    }
                    continue;
                }

                // Increase quanta!
                //{
                //    auto mv = gcode(group);
                //    if (mv > 1) {
                //        fprintf(stderr, "%llu vs %llu %llu\n", uint64_t(maxval / mv), uint64_t(maxval), uint64_t(mv));
                //    }
                //}

                if (maxval < 4) { // Doesn't always have 2 detection bits
                    if (runbits[c] == 1)
                        s.push(0u, 1); // Same, just one bit
                    else {
                        s.push(3u, ubits + 1); // change flag, + 1 as ubit len
                        runbits[c] = 1;
                    }
                    for (auto it : group)
                        if (it < 2)
                            s.push(it * 3u, 1ull + it);
                        else
                            s.push(((it & 1) << 2) | 1u, 3);
                    continue;
                }

                // Number of bits after the fist 1
                size_t bits = ilogb(maxval);
                if (runbits[c] == bits)
                    s.push(0u, 1); // Same as before, just a zero bit
                else {
                    s.push(bits * 2 | 1, ubits + 1); // change bit + len on ubits
                    runbits[c] = bits;
                }

                // Three length encoding
                // The bottom bits are read as a group
                // which starts with the two detection bits
                for (uint64_t val : group) {
                    if (val <= mask[bits - 1]) { // short codewords
                        val |= 1ull << (bits - 1);
                        s.push(val, bits); // Starts with 1
                    }
                    else if (val <= mask[bits]) { // Second quarter, nominal
                        val = (val >> 1) | ((val & 1) << bits);
                        s.push(val, bits + 1); // starts with 01, rotated right one bit
                    }
                    else { // last half, least likely, long codewords
                        val &= mask[bits];
                        if (bits < 63) {
                            val = (val >> 2) | ((val & 0b11) << bits);
                            s.push(val, bits + 2); // starts with 00, rotated two bits
                        }
                        else { // bits == 63, can't push 65 bits in one call
                            s.push(val >> 2, bits);
                            s.push(val & 0b11, 2);
                        }
                    }
                }
            }
        }
    }
    return s.v;
}

template<typename T = uint8_t>
std::vector<T> unsin(std::vector<uint8_t>& src, size_t xsize, size_t ysize, 
    size_t bands, size_t bsize, int mb = 1)
{
    std::vector<T> image(xsize * ysize * bands);
    Bitstream s(src);
    std::vector<T> prev(bands, 0);
    std::vector<T> group(bsize * bsize);
    const uint8_t* xlut = xx[bsize];
    const uint8_t* ylut = yy[bsize];
    const constexpr int ubits = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    std::vector<size_t> runbits(bands, sizeof(T) * 8 - 1);

    std::vector<size_t> offsets(group.size());
    for (size_t i = 0; i < offsets.size(); i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;

    for (size_t y = 0; (y + bsize) <= ysize; y += bsize) {
        for (size_t x = 0; (x + bsize) <= xsize; x += bsize) {
            size_t loc = (y * xsize + x) * bands;
            for (int c = 0; c < bands; c++) {
                uint64_t bits = runbits[c];
                if (0 != s.get()) { // The bits change flag
                    s.pull(runbits[c], ubits);
                    bits = runbits[c];
                }
                uint64_t val;
                if (0 == bits) { // 0 or 1
                    if (0 == s.get()) // All 0s
                        fill(group.begin(), group.end(), 0);
                    else { // 0 and 1s
                        s.pull(val, group.size());
                        for (auto& it : group) {
                            it = val & 1;
                            val >>= 1;
                        }
                    }
                }
                else if (1 == bits) { // 2 bits nominal, could be a single bit
                    for (auto& it : group)
                        if ((it = static_cast<T>(s.get())))
                            if (!(it = static_cast<T>(s.get())))
                                it = static_cast<T>(0b10 + s.get());
                }
                else { // triple length
                    for (auto& it : group) {
                        s.pull(it, bits);
                        if (it > mask[bits - 1]) // Starts with 1
                            it &= mask[bits - 1];
                        else if (it > mask[bits - 2])
                            it = static_cast<T>(s.get() + (it << 1));
                        else {
                            s.pull(val, 2);
                            it = static_cast<T>((1ull << bits) + (it << 2) + val);
                        }
                    }
                }
                prev[c] = undsign(group, prev[c]);
                for (size_t i = 0; i < group.size(); i++)
                    image[loc + c + offsets[i]] = group[i];
            }
            for (int c = 0; c < bands; c++)
                if (mb >= 0 && mb != c)
                    for (size_t i = 0; i < group.size(); i++)
                        image[loc + c + offsets[i]] += image[loc + mb + offsets[i]];
        }
    }
    return image;
}

} // Namespace SiBi
