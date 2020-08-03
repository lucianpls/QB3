// SiBi.cpp : Test only
//

#include <iostream>
#include <cmath>
#include <algorithm>
#include <chrono>

#include "bmap.h"
#include "denc.h"

using namespace std;
using namespace chrono;
using namespace SiBi;

template<typename T>
vector<T> to(vector<uint8_t> &v, T m) {
    vector<T> result;
    result.reserve(v.size());
    for (auto it : v)
        result.push_back(m * it);
    return result;
}

template<typename T>
void check(vector<uint8_t> &image, size_t bsize, int m) {
    size_t xsize = 3776;
    size_t ysize = 2520;
    size_t bands = 3;
    high_resolution_clock::time_point t1, t2;
    double time_span;

    auto img = to(image, static_cast<T>(m));
    //t1 = high_resolution_clock::now();
    //auto v = truncode(img, xsize, ysize, bsize);
    //t2 = high_resolution_clock::now();
    //time_span = duration_cast<duration<double>>(t2 - t1).count();
    //cout << endl
    //    << sizeof(T) * 8 << " Size is " << v.size() << endl
    //    << "Compressed to " << float(v.size()) * 100 / image.size() / sizeof(T) << endl
    //    << "Took " << time_span << " seconds" << endl;

    //t1 = high_resolution_clock::now();
    //auto re = untrun<T>(v, xsize, ysize, bands, bsize);
    //t2 = high_resolution_clock::now();
    //time_span = duration_cast<duration<double>>(t2 - t1).count();
    //cout << "Untrun took " << time_span << " seconds" << endl;

    //for (size_t i = 0; i < img.size(); i++)
    //    if (img[i] != re[i])
    //        cout << "Difference at " << i << " " 
    //        << img[i] << " " << re[i] << endl;

    t1 = high_resolution_clock::now();
    auto v = sincode(img, xsize, ysize, bsize, 2);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "Sincode " << sizeof(T) * 8 << " size is " << v.size() << endl
        << "Compressed to " << float(v.size()) * 100 / image.size() / sizeof(T) << endl
        << "Took " << time_span << " seconds" << endl;

    t1 = high_resolution_clock::now();
    auto re = unsin<T>(v, xsize, ysize, bands, bsize, 2);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "UnSin took " << time_span << " seconds" << endl;

    for (size_t i = 0; i < img.size(); i++)
        if (img[i] != re[i])
            cout << "Difference at " << i << " " 
            << img[i] << " " << re[i] << endl;
}

int main()
{
    //int sx = 200, sy = 299;
    //BMap bm(sx, sy);
    ////std::cout << bm.bit(7, 8);
    ////bm.clear(7, 8);
    ////std::cout << bm.bit(7, 8);
    //////bm.set(7, 8);
    //////std::cout << bm.bit(7, 8);
    ////std::cout << std::endl;

    //// a rectangle
    //for (int y = 30; y < 60; y++)
    //    for (int x = 123; x < 130; x++)
    //        bm.clear(x, y);

    //// circles
    //int cx = 150, cy = 76, cr = 34;
    //for (int y = 0; y < sy; y++)
    //    for (int x = 0; x < sx; x++)
    //        if (((x - cx) * (x - cx) + (y - cy) * (y - cy)) < cr * cr)
    //            bm.clear(x, y);

    //cx = 35; cy = 212; cr = 78;
    //for (int y = 0; y < sy; y++)
    //    for (int x = 0; x < sx; x++)
    //        if (((x - cx) * (x - cx) + (y - cy) * (y - cy)) < cr * cr)
    //            bm.clear(x, y);

    //cx = 79; cy = 235; cr = 135;
    //for (int y = 0; y < sy; y++)
    //    for (int x = 0; x < sx; x++)
    //        if (((x - cx) * (x - cx) + (y - cy) * (y - cy)) < cr * cr)
    //            bm.clear(x, y);

    //Bitstream s;
    //std::cout << std::endl << bm.dsize() * 8 << std::endl;
    //bm.pack(s);
    ////bm.dump(std::string("file.raw"));
    //std::cout << s.v.size() * 8 << std::endl;
    //auto v(s.v);
    //RLE(s.v, s.v);
    //std::cout << s.v.size() * 8 << std::endl;
    //unRLE(s.v, s.v);
    //std::cout << s.v.size() * 8 << std::endl;
    //for (int i = 0; i < v.size(); i++)
    //    if (v[i] != s.v[i])
    //        break;

    //BMap bm1(sx, sy);
    //bm1.unpack(s);
    //bm1.compare(bm);

    // Now for the RGB image, read from a PNM file
    // Image is 2550x3776x3, starts at offset 16
    FILE* f;
    if (fopen_s(&f, "input.pnm", "rb") || !f) {
        cerr << "Can't open input file";
        exit(errno);
    }
    fseek(f, 17, SEEK_SET);
    size_t xsize = 3776;
    size_t ysize = 2520;
    size_t bands = 3;
    vector<uint8_t> image(xsize * ysize * bands);
    fread(image.data(), ysize * bands, xsize, f);
    fclose(f);
    int bsize = 4;
    check<uint64_t>(image, bsize, 5);
    check<uint32_t>(image, bsize, 5);
    check<uint16_t>(image, bsize, 5);
    check<uint8_t>(image, bsize, 1);
}