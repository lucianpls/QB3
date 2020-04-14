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

vector<uint16_t> toshort(vector<uint8_t> &v) {
    vector<uint16_t> result;
    for (auto it : v)
        result.push_back(5 * it);
    return result;
}

vector<uint32_t> tolong(vector<uint8_t>& v) {
    vector<uint32_t> result;
    for (auto it : v)
        result.push_back(5 * it);
    return result;
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
    int xsize = 3776;
    int ysize = 2520;
    int bands = 3;
    vector<uint8_t> image(xsize * ysize * bands);
    fread(image.data(), ysize * bands, xsize, f);
    fclose(f);

    int bsize = 4;

    high_resolution_clock::time_point t1, t2;
    vector<uint8_t> v;
    vector<uint8_t> reimage;
    double time_span;

    // Image is rotated
    t1 = high_resolution_clock::now();
    v = truncode(image, 3776, 2520, bsize);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "Size is " << v.size() << endl;
    cout << "Took " << time_span << " seconds" << endl;

    t1 = high_resolution_clock::now();
    reimage = untrun(v, 3776, 2520, 3, bsize);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "UnTrun took " << time_span << " seconds" << endl;

    for (int i = 0; i < image.size(); i++)
        if (image[i] != reimage[i])
            cout << "Difference at " << i << endl;

    t1 = high_resolution_clock::now();
    v = sincode(image, 3776, 2520, bsize);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "Size is " << v.size() << endl;
    cout << "Took " << time_span << " seconds" << endl;

    t1 = high_resolution_clock::now();
    reimage = unsin(v, 3776, 2520, 3, bsize);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "UnSin took " << time_span << " seconds" << endl;

    for (int i = 0; i < image.size(); i++)
        if (image[i] != reimage[i])
            cout << "Difference at " << i << endl;

    vector<uint16_t> img16 = toshort(image);
    t1 = high_resolution_clock::now();
    auto v16 = truncode(img16, 3776, 2520, bsize);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "11bit Size is " << v16.size() << endl;
    cout << "Took " << time_span << " seconds" << endl;

    t1 = high_resolution_clock::now();
    auto re16 = untrun<uint16_t>(v16, 3776, 2520, 3, bsize);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "Untrun took " << time_span << " seconds" << endl;

    for (int i = 0; i < img16.size(); i++)
        if (img16[i] != re16[i])
            cout << "Difference at " << i
            << " " << img16[i] << " " << re16[i]
            << endl;

    t1 = high_resolution_clock::now();
    v16 = sincode(img16, 3776, 2520, bsize);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "11bit Size is " << v16.size() << endl;
    cout << "Took " << time_span << " seconds" << endl;

    t1 = high_resolution_clock::now();
    re16 = unsin<uint16_t>(v16, 3776, 2520, 3, bsize);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "UnSin took " << time_span << " seconds" << endl;

    for (int i = 0; i < img16.size(); i++)
        if (img16[i] != re16[i])
            cout << "Difference at " << i
            << " " << img16[i] << " " << re16[i]
            << endl;

    vector<uint32_t> img32 = tolong(image);
    t1 = high_resolution_clock::now();
    auto v32 = truncode(img32, 3776, 2520, bsize);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "11bit Size is " << v32.size() << endl;
    cout << "Took " << time_span << " seconds" << endl;

    t1 = high_resolution_clock::now();
    auto re32 = untrun<uint32_t>(v32, 3776, 2520, 3, bsize);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "Untrun took " << time_span << " seconds" << endl;

    for (int i = 0; i < img32.size(); i++)
        if (img32[i] != re32[i])
            cout << "Difference at " << i
            << " " << img32[i] << " " << re32[i]
            << endl;

    t1 = high_resolution_clock::now();
    v32 = sincode(img32, 3776, 2520, bsize);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "11bit Size is " << v32.size() << endl;
    cout << "Took " << time_span << " seconds" << endl;

    t1 = high_resolution_clock::now();
    re32 = unsin<uint32_t>(v32, 3776, 2520, 3, bsize);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << "UnSin took " << time_span << " seconds" << endl;

    for (int i = 0; i < img32.size(); i++)
        if (img32[i] != re32[i])
            cout << "Difference at " << i
            << " " << img32[i] << " " << re32[i]
            << endl;
}