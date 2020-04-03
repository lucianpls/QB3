#pragma once
#include <vector>
#include <cinttypes>
#include <limits>

namespace SiBi {
#define MASK(v) ((1ull << v)-1)
    const uint64_t mask[65] = {
         MASK(0),  MASK(1),  MASK(2),  MASK(3),  MASK(4),  MASK(5),  MASK(6),  MASK(7),
         MASK(8),  MASK(9), MASK(10), MASK(11), MASK(12), MASK(13), MASK(14), MASK(15),
        MASK(16), MASK(17), MASK(18), MASK(19), MASK(20), MASK(21), MASK(22), MASK(23),
        MASK(24), MASK(25), MASK(26), MASK(27), MASK(28), MASK(29), MASK(30), MASK(31),
        MASK(32), MASK(33), MASK(34), MASK(35), MASK(36), MASK(37), MASK(38), MASK(39),
        MASK(40), MASK(41), MASK(42), MASK(43), MASK(44), MASK(45), MASK(46), MASK(47),
        MASK(48), MASK(49), MASK(50), MASK(51), MASK(52), MASK(53), MASK(54), MASK(55),
        MASK(56), MASK(57), MASK(58), MASK(59), MASK(60), MASK(61), MASK(62), MASK(63),
        ~0
    };
#undef MASK

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
        size_t xsize, size_t ysize, int bsize)
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
        size_t xsize, size_t ysize, int bsize)
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
                size_t loc = (y * xsize + x) * 3; // Top-left pixel
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
