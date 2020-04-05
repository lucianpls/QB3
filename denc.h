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

#define HALFMAX(T) (std::numeric_limits<T>::max() >> 1)

    template<typename T = uint8_t>
    static T dsign(std::vector<T>& v, T prev) {
        assert(std::is_integral<T>::value);
        assert(std::is_unsigned<T>::value);
        for (auto& it : v) {
            std::swap(it, prev);
            it = prev - it;
            it = (it > HALFMAX(T)) ? -(2 * it + 1) : (2 * it);
        }
        return prev;
    }

    template<typename T = uint8_t>
    static T undsign(std::vector<T>& v, T prev) {
        assert(std::is_integral<T>::value);
        assert(std::is_unsigned<T>::value);
        for (auto& it : v)
            prev = it = prev + ((it & 1) ? (-(it >> 1) - 1) : (it >> 1));
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
    std::vector<T> truncode(std::vector<T>& image,
        size_t xsize, size_t ysize, int bsize)
    {
        assert(std::is_integral<T>::value);
        assert(std::is_unsigned<T>::value);
        std::vector<T> result;
        Bitstream s(result);
        // The sizes are multiples of 8, no need to check
        size_t bands = image.size() / xsize / ysize;
        std::vector<T> prev(bands, HALFMAX(T));

        const uint8_t* xlut = xx[bsize];
        const uint8_t* ylut = yy[bsize];
        std::vector<T> group(bsize * bsize);
        for (int y = 0; (y + bsize) <= ysize; y += bsize) {
            for (int x = 0; (x + bsize) <= xsize; x += bsize) {
                size_t loc = (y * xsize + x) * bands;
                for (int c = 0; c < bands; c++) {

                    for (int i = 0; i < group.size(); i++)
                        group[i] = image[loc + c + (ylut[i] * xsize + xlut[i]) * bands];

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
                    int bits = ilogb(maxval);

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
                        for (auto val : group)
                            s.push(val, bits + 1);
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

    template<typename T = uint8_t>
    std::vector<T> untrun(std::vector<T>& src,
        size_t xsize, size_t ysize, size_t bands, int bsize)
    {
        assert(std::is_integral<T>::value);
        assert(std::is_unsigned<T>::value);
        std::vector<T> image(xsize * ysize * bands);
        Bitstream s(src);
        std::vector<T> prev(bands, HALFMAX(T));
        std::vector<T> group(bsize * bsize);
        const uint8_t* xlut = xx[bsize];
        const uint8_t* ylut = yy[bsize];

        for (int y = 0; (y + bsize) <= ysize; y += bsize) {
            for (int x = 0; (x + bsize) <= xsize; x += bsize) {
                size_t loc = (y * xsize + x) * bands;
                for (int c = 0; c < bands; c++) {
                    int bits;
                    s.pull(bits, 3);
                    if (0 == bits) {
                        uint64_t val;
                        s.pull(val, group.size());
                        for (auto& it : group) {
                            it = val & 1;
                            val >>= 1;
                        }
                    }
                    else {
                        uint64_t maxval;
                        s.pull(maxval, bits - 1);
                        maxval = (1ull << bits) | (maxval << 1) | 1;
                        auto cutof = mask[bits + 1] - maxval;
                        if (0 == cutof) {
                            for (auto& it : group)
                                s.pull(it, bits + 1);
                        }
                        else { // Default case, truncated
                            for (auto& it : group) {
                                s.pull(it, bits);
                                if (it >= cutof) {
                                    T bit;
                                    s.pull(bit, 1);
                                    it = (it << 1) + bit - cutof;
                                }
                            }
                        }
                    }

                    prev[c] = undsign(group, prev[c]);
                    for (int i = 0; i < group.size(); i++)
                        image[loc + c + (ylut[i] * xsize + xlut[i]) * bands] = group[i];
                }
            }
        }
        return image;
    }


    // Encoding with predefined Huffman tables
    // Does not need the maxvalue to be encoded, only 
    // the reference number of bits
    template <typename T = uint8_t>
    std::vector<T> sincode(std::vector<T>& image,
        size_t xsize, size_t ysize, int bsize)
    {
        assert(std::is_integral<T>::value);
        assert(std::is_unsigned<T>::value);
        std::vector<T> result;
        Bitstream s(result);
        const uint8_t* xlut = xx[bsize];
        const uint8_t* ylut = yy[bsize];
        size_t bands = image.size() / xsize / ysize;
        std::vector<T> prev(bands, HALFMAX(T));
        std::vector<T> group(bsize * bsize);
        for (int y = 0; (y + bsize) <= ysize; y += bsize) {
            for (int x = 0; (x + bsize) <= xsize; x += bsize) {
                size_t loc = (y * xsize + x) * bands; // Top-left pixel
                for (int c = 0; c < bands; c++) {

                    for (int i = 0; i < group.size(); i++)
                        group[i] = image[loc + c + (ylut[i] * xsize + xlut[i]) * bands];

                    // Delta with low sign encode
                    prev[c] = dsign(group, prev[c]);
                    uint64_t maxval = 1 | *max_element(group.begin(), group.end());

                    // Number of bits after the fist 1
                    size_t bits = ilogb(maxval);
                    s.push(bits, 3);
                    if (0 == bits) { // Best case, all values are 0 or 1
                        uint64_t val = 0;
                        // Write backwards, will be read forward
                        for (auto it = group.rbegin(); it != group.rend(); it++)
                            val = val * 2 + *it;
                        s.push(val, group.size());
                        continue;
                    }

                    // Doesn't always have 2 bits
                    if (1 == bits) {
                        for (auto it : group) {
                            if (it < 2)
                                s.push(1 << it, 1 + it);
                            else
                                s.push((it & 1) << 2, 3);
                        }
                        continue;
                    }

                    // Use the variable length encoding
                    // Only need to push the number of bits, not the maxvalue
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

    template<typename T = uint8_t>
    std::vector<T> unsin(std::vector<T>& src,
        size_t xsize, size_t ysize, size_t bands, int bsize)
    {
        assert(std::is_integral<T>::value);
        assert(std::is_unsigned<T>::value);
        std::vector<T> image(xsize * ysize * bands);
        Bitstream s(src);
        std::vector<T> prev(bands, HALFMAX(T));
        std::vector<T> group(bsize * bsize);
        const uint8_t* xlut = xx[bsize];
        const uint8_t* ylut = yy[bsize];

        for (int y = 0; (y + bsize) <= ysize; y += bsize) {
            for (int x = 0; (x + bsize) <= xsize; x += bsize) {
                size_t loc = (y * xsize + x) * bands;
                for (int c = 0; c < bands; c++) {
                    uint64_t val;
                    int bits;
                    s.pull(bits, 3);
                    switch (bits) {
                    case 0: {
                        s.pull(val, group.size());
                        for (auto& it : group) {
                            it = val & 1;
                            val >>= 1;
                        }
                        break;
                    }
                    case 1: { // Don't have the two detection bits
                        for (auto& it : group) {
                            s.pull(val, 1);
                            if (val)
                                it = 0;
                            else {
                                s.pull(val, 1);
                                if (val) {
                                    it = 1;
                                }
                                else {
                                    s.pull(val, 1);
                                    it = 0b10 | val;
                                }
                            }
                        }
                        break;
                    }
                    default: {
                        for (auto& it : group) {
                            uint64_t val;
                            s.pull(val, bits);
                            if (val > mask[bits - 1]) { // Starts with 1
                                it = val & mask[bits - 1];
                            }
                            else if (val > mask[bits - 2]) { // Starts with 00
                                it = val << 1;
                                s.pull(val, 1);
                                it += val;
                            }
                            else {
                                it = val << 2;
                                s.pull(val, 2);
                                it += val | (1ull << bits);
                            }
                        }
                    }
                    } // switch

                    prev[c] = undsign(group, prev[c]);
                    for (int i = 0; i < group.size(); i++)
                        image[loc + c + (ylut[i] * xsize + xlut[i]) * bands] = group[i];
                }
            }
        }
        return image;
    }

} // Namespace SiBi
