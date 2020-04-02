#pragma once
#include <vector>
#include <cinttypes>
#include <limits>

namespace SiBi {
    const uint64_t mask[33] = {
        0x0, 0x1, 0x3, 0x7,
        0xf, 0x1f, 0x3f, 0x7f,
        0xff, 0x1ff, 0x3ff, 0x7ff,
        0xfff, 0x1fff, 0x3fff, 0x7fff,
        0xffff, 0x1ffff, 0x3ffff, 0x7ffff,
        0xfffff, 0x1fffff, 0x3fffff, 0x7fffff,
        0xffffff, 0x1ffffff, 0x3ffffff, 0x7ffffff,
        0xfffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
        0xffffffff
    };

    template<typename T = uint8_t>
    static T dsign(std::vector<T>& v, T prev) {
        assert(std::is_integral<T>::value);
        assert(std::is_unsigned<T>::value);
        static T maxv = std::numeric_limits<T>::max() >> 1;
        for (auto& it : v) {
            std::swap(it, prev);
            it = prev - it;
            it = (it > maxv) ? -(2 * it + 1) : (2 * it);
        }
        return prev;
    }

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

    // Block size should be 8
    // for noisy images 4 is better
    // 2 generates too much overhead
    // 16 might work for slow varying inputs, 
    // which would need the lookup tables extended
    // Works for non-power of two blocks, using custom tables

    template<typename T = uint8_t>
    std::vector<T> encode(std::vector<T>& image,
        size_t xsize, size_t ysize, size_t bsize)
    {
        assert(std::is_integral<T>::value);
        assert(std::is_unsigned<T>::value);

        Bitstream s;
        // The sizes are multiples of 8, no need to check
        size_t bands = image.size() / xsize / ysize;
        std::vector<T> prev(bands, 127); // RGB

        const uint8_t* xlut = xx[bsize];
        const uint8_t* ylut = yy[bsize];
        std::vector<T> group(bsize * bsize);
        for (int y = 0; (y + bsize) <= ysize; y += bsize) {
            for (int x = 0; (x + bsize) <= xsize; x += bsize) {
                size_t loc = (y * xsize + x) * 3;
                for (int c = 0; c < 3; c++) {

                    for (int i = 0; i < group.size(); i++)
                        group[i] = image[loc + c + (ylut[i] * xsize + xlut[i]) * 3];

                    // Delta with low sign encode
                    prev[c] = dsign(group, prev[c]);

                    // Round up to nearest odd (negative) value
                    // Saves writing that last bit on every block
                    // Expends one extra bit on two specific values
                    // if they are present, by moving the cutof value 
                    // down by one
                    // The net effect decreases output size
                    uint64_t maxval = 1 | *max_element(group.begin(), group.end());

                    // Encode the maxval
                    // Number of bits after the fist 1
                    size_t bits = ilogb(maxval);

                    if (0 == bits) { // Best case, all values are 0 or 1
                        s.push(0, 3);
                        uint64_t val = 0;
                        for (auto it = group.rbegin(); it != group.rend(); it++)
                            val = val * 2 + *it;
                        s.push(val, group.size());
                        continue;
                    }

                    // Push the middle bits of maxval, prefixed by the number of bits
                    s.push((maxval & (mask[bits] - 1)) * 4 + bits, bits + 2);

                    // Truncated binary encoding
                    auto cutof = mask[bits + 1] - maxval;
                    if (0 == cutof) { // Uniform length codewords
                        bits++;
                        for (auto val : group)
                            s.push(val, bits);
                        continue;
                    }

                    for (uint64_t val : group) {
                        if (val < cutof) { // Truncated
                            s.push(val, bits);
                            continue;
                        }
                        // Adjust, rotate and store the value
                        val += cutof;
                        val = (val >> 1) + ((val & 1) << bits);
                        s.push(val, bits + 1);
                    }
                }
            }
        }
        return s.v;
    }


    // Encoding with predefined Huffman tables
    // Does not need the maxvalue to be encoded, only 
    // the reference number of bits
    template <typename T = uint8_t>
    std::vector<T> siencode(std::vector<T>& image,
        size_t xsize, size_t ysize, size_t bsize)
    {
        assert(std::is_integral<T>::value);
        assert(std::is_unsigned<T>::value);

        Bitstream s;
        // The sizes are multiples of 8, no need to check
        const uint8_t* xlut = xx[bsize];
        const uint8_t* ylut = yy[bsize];
        size_t bands = image.size() / xsize / ysize;
        std::vector<T> prev(bands, 127); // RGB
        std::vector<T> group(bsize * bsize);
        for (int y = 0; (y + bsize) <= ysize; y += bsize) {
            for (int x = 0; (x + bsize) <= xsize; x += bsize) {
                size_t loc = (y * xsize + x) * 3;
                for (int c = 0; c < 3; c++) {

                    for (int i = 0; i < group.size(); i++)
                        group[i] = image[loc + c + (ylut[i] * xsize + xlut[i]) * 3];

                    // Delta with low sign encode
                    prev[c] = dsign(group, prev[c]);
                    uint64_t maxval = 1 | *max_element(group.begin(), group.end());

                    // Number of bits after the fist 1
                    size_t bits = ilogb(maxval);
                    if (0 == bits) { // Best case, all values are 0 or 1
                        s.push(0, 3);
                        uint64_t val = 0;
                        // Scan backwards, will be read forward
                        for (auto it = group.rbegin(); it != group.rend(); it++)
                            val = val * 2 + *it;
                        s.push(val, group.size());
                        continue;
                    }

                    // Use the variable length encoding
                    // Only need to push the number of bits, not the maxvalue
                    s.push(bits, 3);
                    for (uint64_t val : group) {
                        if (val <= mask[bits - 1]) {
                            // Lowest quarter, one bit short codewords, most likely
                            val |= 1ull << (bits - 1);
                            s.push(val, bits); // Starts with 1
                        }
                        else if (val <= mask[bits]) { // Second quarter
                            // starts with 01, then rotated
                            val = (val >> 1) | ((val & 1) << bits);
                            s.push(val, bits + 1);
                        }
                        else { // Last half, least likely
                            val &= mask[bits];
                            // starts with 00, rotated two bits
                            val = (val >> 2) | ((val & 0b11) << bits);
                            s.push(val, bits + 2);
                        }
                    }
                }
            }
        }
        return s.v;
    }

} // Namespace SiBi
