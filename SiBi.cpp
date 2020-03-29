// SiBi.cpp : Test only
//

#include <iostream>
#include <cmath>
#include <algorithm>

#include "bmap.h"
#include "denc.h"

using namespace std;

static void dump8x8(vector<uint8_t>& v) {
    cout << hex;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            cout << int(v[y * 8 + x]) << ",";
        }
        cout << endl;
    }
    cout << endl;
    cout << dec;
}

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

    fopen_s(&f, "minvals.raw", "wb");
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
    // This should be 8, but for noisy images 4 is better
    // 16 might work for slow varying inputs, 
    // which would need the lookup tables extended
    static const int bsize = 8;
    vector<uint8_t> group(bsize * bsize);
    for (int y = 0; y < 2520; y += bsize) {
        for (int x = 0; x < 3776; x += bsize) {
            size_t loc = (y * line_sz + x) * 3;
            for (int c = 0; c < 3; c++) {
                for (int i = 0; i < group.size(); i++) {
                    static const uint8_t xlut[64] = {
                        0, 1, 0, 1, 2, 3, 2, 3,
                        0, 1, 0, 1, 2, 3, 2, 3,
                        4, 5, 4, 5, 6, 7, 6, 7,
                        4, 5, 4, 5, 6, 7, 6, 7,
                        0, 1, 0, 1, 2, 3, 2, 3,
                        0, 1, 0, 1, 2, 3, 2, 3,
                        4, 5, 4, 5, 6, 7, 6, 7,
                        4, 5, 4, 5, 6, 7, 6, 7
                    };
                    static const uint8_t ylut[64] = {
                        0, 0, 1, 1, 0, 0, 1, 1,
                        2, 2, 3, 3, 2, 2, 3, 3,
                        0, 0, 1, 1, 0, 0, 1, 1,
                        2, 2, 3, 3, 2, 2, 3, 3,
                        4, 4, 5, 5, 4, 4, 5, 5,
                        6, 6, 7, 7, 6, 6, 7, 7,
                        4, 4, 5, 5, 4, 4, 5, 5,
                        6, 6, 7, 7, 6, 6, 7, 7
                    };
                    group[i] = image[loc + c + (ylut[i] * line_sz + xlut[i]) * 3];
                }

                //dump8x8(group);
                // Do the delta encoding on this group
                Denc deltaenc(1);
                deltaenc.prev[0] = prev[c];
                deltaenc.delta(group);
                //dump8x8(deltaenc.v);
                prev[c] = deltaenc.prev[0];
                deltaenc.recode();
                //dump8x8(group);

                uint64_t maxval = *max_element(deltaenc.v.begin(), deltaenc.v.end());
                //dump8x8(group);
                //dump8x8(deltaenc.v);
                group.swap(deltaenc.v);
                fwrite(group.data(), 1, group.size(), f);
                hist[maxval]++;
                //for (auto it : group)
                //    hist[it]++;

                // Encode the maxval
                // Number of bits after the fist 1
                size_t bits = maxval ? ilogb(maxval) : 0;
                bithist[bits]++;
                // Push the low bits of maxval, prefixed by the number of bits
                s.push((maxval & Bitstream::mask[bits]) * 8 + bits, bits + 3);

                // Encode the 64values
                // Best case, maxval is 0 or 1
                if (0 == bits) {
                    uint64_t val = 0;
                    if (maxval) // Gather the bits, in low endian order
                        for (auto it = group.rbegin(); it != group.rend(); it++)
                            val = val * 2 + *it;
                    s.push(val, group.size());
                    continue;
                }

                auto cutof = Bitstream::mask[bits + 1] - maxval;
                if (0 == cutof) { // Uniform length codewords
                    for (auto it = group.begin(); it != group.end(); it++)
                        s.push(*it, bits + 1);
                    continue;
                }

                for (auto it = group.begin(); it != group.end(); it++) {
                    uint64_t val = *it;
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

    fclose(f);

    fopen_s(&f, "out.raw", "wb");
    fwrite(s.v.data(), 1, s.v.size(), f);
    for (int i = 0; i < hist.size(); i++)
        cout << i << "," << hist[i] << endl;

    for (int i = 0; i < bithist.size(); i++)
        cout << i << "," << bithist[i] << endl;
    cout << s.v.size() << endl;
    fclose(f);
}
