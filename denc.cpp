#include "denc.h"
#include "bmap.h"
#include <iostream>
#include <limits>

using namespace std;


// Delta and by low sign encoding
static int dsign(std::vector<uint8_t>& v, uint8_t prev) {
    for (auto& it : v) {
        swap(it, prev);
        it = prev - it;
        it = (it & 0x80) ? -(2 * it + 1) : (2 * it);
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

// Block size should be 8, for noisy 
// images 4 is better
// 2 generates too much overhead
// 16 might work for slow varying inputs, 
// which would need the lookup tables extended
// Works for non-power of two blocks, using custom tables

std::vector<uint8_t> encode(std::vector<uint8_t> &image,
    size_t xsize, size_t ysize, size_t bsize)
{
    Bitstream s;
    vector<size_t> hist(256);
    vector<size_t> bithist(8);
    // The sizes are multiples of 8, no need to check
    size_t bands = image.size() / xsize / ysize;
    vector<uint8_t> prev(bands, 127); // RGB

    const uint8_t* xlut = xx[bsize];
    const uint8_t* ylut = yy[bsize];
    vector<uint8_t> group(bsize * bsize);
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
                bithist[bits]++;
                hist[maxval]++;

                if (0 == bits) { // Best case, all values are 0 or 1
                    s.push(0, 3);
                    uint64_t val = 0;
                    for (auto it = group.rbegin(); it != group.rend(); it++)
                        val = val * 2 + *it;
                    s.push(val, group.size());
                    continue;
                }

                // Push the middle bits of maxval, prefixed by the number of bits
                s.push((maxval & (Bitstream::mask[bits] - 1)) * 4 + bits, bits + 2);

                // Truncated binary encoding
                auto cutof = Bitstream::mask[bits + 1] - maxval;
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
std::vector<uint8_t> siencode(std::vector<uint8_t>& image,
    size_t xsize, size_t ysize, size_t bsize)
{
    Bitstream s;
    vector<size_t> hist(256);
    vector<size_t> bithist(8);
    // The sizes are multiples of 8, no need to check
    const uint8_t* xlut = xx[bsize];
    const uint8_t* ylut = yy[bsize];
    size_t bands = image.size() / xsize / ysize;
    vector<uint8_t> prev(bands, 127); // RGB
    vector<uint8_t> group(bsize * bsize);
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
                bithist[bits]++;
                hist[maxval]++;

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
                    if (val <= Bitstream::mask[bits - 1]) {
                        // Lowest quarter, one bit short codewords, most likely
                        s.push(val |(1ull << (bits -1)), bits); // Starts with 1
                    }
                    else if (val <= Bitstream::mask[bits]) { // Second quarter
                        val = (val >> 1) | ((val & 1) << bits); // starts with 01, then rotated
                        s.push(val, bits + 1);
                    }
                    else {
                        // Last half, one bit longer codewords, least likely, starts with 00
                        val &= Bitstream::mask[bits];
                        // starts with 00, Rotated two bits, 
                        val = (val >> 2) | ((val & 0b11) << bits);
                        s.push(val, bits + 2);
                    }
                }
            }
        }
    }
    return s.v;
}