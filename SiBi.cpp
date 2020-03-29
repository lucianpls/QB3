// SiBi.cpp : Test only
//

#include <iostream>
#include <cmath>
#include <algorithm>

#include "bmap.h"
#include "denc.h"

using namespace std;

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
    // Image is 3776x2520x3, starts at offset 16
    FILE* f;
    if (fopen_s(&f, "input.pnm", "rb") || ! f) {
        cerr << "Can't open input file";
        exit(errno);
    }
    fseek(f, 17, SEEK_SET);
    vector<uint8_t> image(3776*2520*3);
    fread(image.data(), 3776, 2520 * 3, f);
    fclose(f);
    // 

    Denc deltaenc;
    deltaenc.delta(image);
    deltaenc.deltacheck(image);
    deltaenc.recode();
    // This isn't right, just to get an idea
    vector<uint8_t>& source = deltaenc.v;

    s.clear();
    for (size_t loc = 0; (loc + 64 * 3) <= source.size(); loc += 64 * 3) {
        for (int c = 0; c < 3; c++) {
            vector<uint8_t> group;
            for (int i = 0; i < 64; i++)
                group.push_back(source[loc + i * 3 + c]);
            uint64_t maxval = *max_element(group.begin(), group.end());
            // Number of bits after the fist 1
            size_t bits = ilogb(maxval);
            // Push the low bits of maxval, prefixed by the number of bits
            s.push((maxval & Bitstream::mask[bits]) * 8 + bits, bits + 3);
            // encode the values

            // Best case, maxval is 0 or 1
            if (0 == bits) {
                uint64_t val = 0;
                if (maxval) // Gather the bits, in low endian order
                    for (auto it = group.rbegin(); it != group.rend(); it++)
                        val = val * 2 + *it;
                s.push(val, group.size());
                continue;
            }

            // Worst case, truncated binary doesn't apply
            if (maxval == Bitstream::mask[bits + 1] || (maxval + 1) == Bitstream::mask[bits + 1]) {
                for (auto it = group.begin(); it != group.end(); it++)
                    s.push(*it, bits + 1);
                continue;
            }

            // default case, truncated binary, at least 2 unused values
            auto cutof = Bitstream::mask[bits + 1] - maxval;
            for (auto it = group.begin(); it != group.end(); it++) {
                uint64_t val = *it;
                if (val < cutof) {
                    s.push(val, bits);
                }
                else {
                    val += cutof;
                    // Twist it, move the lowest bit to the highest
                    val = (val >> 1) + ((val & 1) << bits);
                    s.push(val, bits + 1);
                }
            }
        }

    }
    fopen_s(&f, "out.raw", "wb");
    fwrite(s.v.data(), 1, s.v.size(), f);
    fclose(f);
}
