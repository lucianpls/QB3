// SiBi.cpp : Test only
//

#include <iostream>
#include <cmath>
#include <algorithm>

#include "bmap.h"
#include "denc.h"

using namespace std;

//static void dump8x8(vector<uint8_t>& v) {
//    cout << hex;
//    for (int y = 0; y < 8; y++) {
//        for (int x = 0; x < 8; x++) {
//            cout << int(v[y * 8 + x]) << ",";
//        }
//        cout << endl;
//    }
//    cout << endl;
//    cout << dec;
//}

int main()
{
    int sx = 200, sy = 299;
    BMap bm(sx, sy);
    //std::cout << bm.bit(7, 8);
    //bm.clear(7, 8);
    //std::cout << bm.bit(7, 8);
    ////bm.set(7, 8);
    ////std::cout << bm.bit(7, 8);
    //std::cout << std::endl;

    // a rectangle
    for (int y = 30; y < 60; y++)
        for (int x = 123; x < 130; x++)
            bm.clear(x, y);

    // circles
    int cx = 150, cy = 76, cr = 34;
    for (int y = 0; y < sy; y++)
        for (int x = 0; x < sx; x++)
            if (((x - cx) * (x - cx) + (y - cy) * (y - cy)) < cr * cr)
                bm.clear(x, y);

    cx = 35; cy = 212; cr = 78;
    for (int y = 0; y < sy; y++)
        for (int x = 0; x < sx; x++)
            if (((x - cx) * (x - cx) + (y - cy) * (y - cy)) < cr * cr)
                bm.clear(x, y);

    cx = 79; cy = 235; cr = 135;
    for (int y = 0; y < sy; y++)
        for (int x = 0; x < sx; x++)
            if (((x - cx) * (x - cx) + (y - cy) * (y - cy)) < cr * cr)
                bm.clear(x, y);

    Bitstream s;
    std::cout << std::endl << bm.dsize() * 8 << std::endl;
    bm.pack(s);
    //bm.dump(std::string("file.raw"));
    std::cout << s.v.size() * 8 << std::endl;
    auto v(s.v);
    RLE(s.v, s.v);
    std::cout << s.v.size() * 8 << std::endl;
    unRLE(s.v, s.v);
    std::cout << s.v.size() * 8 << std::endl;
    for (int i = 0; i < v.size(); i++)
        if (v[i] != s.v[i])
            break;

    BMap bm1(sx, sy);
    bm1.unpack(s);
    bm1.compare(bm);

    // Now for the RGB image, read from a PNM file
    // Image is 2550x3776x3, starts at offset 16
    FILE* f;
    if (fopen_s(&f, "input.pnm", "rb") || ! f) {
        cerr << "Can't open input file";
        exit(errno);
    }
    fseek(f, 17, SEEK_SET);
    // Image is rotated
    vector<uint8_t> image(3776*2520*3);
    fread(image.data(), 2520 *3, 3776, f);
    fclose(f);
    // 

    //Denc deltaenc;
    //deltaenc.delta(image);
    //deltaenc.deltacheck(image);
    //deltaenc.recode();
    // This isn't right, just to get an idea
    //vector<uint8_t>& source = deltaenc.v;

    s.clear();
    vector<size_t> hist(256);
    vector<size_t> bithist(8);
    // The sizes are multiples of 8, no need to check
    size_t line_sz = 3776;
    vector<uint8_t> prev(3, 127); // RGB
    // This should be 8, for noisy images 4 is better
    // 2 generates too much overhead
    // 16 might work for slow varying inputs, 
    // which would need the lookup tables extended
    // Works for non-power of two blocks, using custom tables

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

    static const int bsize = 3;
    const uint8_t* xlut = xp2;
    const uint8_t* ylut = yp2;
    switch (bsize) {
    case(3):
        xlut = x3;
        ylut = y3;
        break;
    case(5):
        xlut = x5;
        ylut = y5;
        break;
    case(6):
        xlut = x6;
        ylut = y6;
        break;
    case(7):
        xlut = x7;
        ylut = y7;
        break;
    }

    vector<uint8_t> group(bsize * bsize);
    for (int y = 0; (y + bsize) <= 2520; y += bsize) {
        for (int x = 0; (x + bsize) <= line_sz; x += bsize) {
            size_t loc = (y * line_sz + x) * 3;
            for (int c = 0; c < 3; c++) {
                for (int i = 0; i < group.size(); i++) {
                    group[i] = image[loc + c + (ylut[i] * line_sz + xlut[i]) * 3];
                }

                //dump8x8(group);
                // Do the delta encoding on this group
                Denc deltaenc(1);
                deltaenc.prev[0] = prev[c];
                deltaenc.delta(group);

                prev[c] = deltaenc.prev[0];
                deltaenc.recode();
                
#define SHO 1
#if defined(SHO)
                uint64_t maxval = 1 | *max_element(deltaenc.v.begin(), deltaenc.v.end());
                group.swap(deltaenc.v);

                // Encode the maxval
                // Number of bits after the fist 1
                size_t bits = ilogb(maxval);
                // Push the middle bits of maxval, prefixed by the number of bits
                if (bits)
                    s.push((maxval & (Bitstream::mask[bits] - 1)) * 4 + bits, bits + 2);
                else
                    s.push(0, 3);
#else
                uint64_t maxval = *max_element(deltaenc.v.begin(), deltaenc.v.end());
                //dump8x8(group);
                //dump8x8(deltaenc.v);
                group.swap(deltaenc.v);

                // Encode the maxval
                // Number of bits after the fist 1
                size_t bits = maxval ? ilogb(maxval) : 0;
                // Push the low bits of maxval, prefixed by the number of bits
                s.push((maxval & Bitstream::mask[bits]) * 8 + bits, bits + 3);
#endif

                bithist[bits]++;
                hist[maxval]++;

                // Encode the 64values
                // Best case, maxval is 0 or 1
                if (0 == bits) {
                    uint64_t val = 0;
                    for (auto it = group.rbegin(); it != group.rend(); it++)
                        val = val * 2 + *it;
                    s.push(val, group.size());
                    continue;
                }

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
                    }
                    else {
                        val += cutof;
                        // Store the rotated value
                        val = (val >> 1) + ((val & 1) << bits);
                        s.push(val, bits + 1);
                    }
                }
            }
        }
    }

    fopen_s(&f, "out.raw", "wb");
    fwrite(s.v.data(), 1, s.v.size(), f);
    for (int i = 0; i < hist.size(); i++)
        cout << i << "," << hist[i] << endl;

    for (int i = 0; i < bithist.size(); i++)
        cout << i << "," << bithist[i] << endl;
    cout << s.v.size() << endl;
    fclose(f);
}
