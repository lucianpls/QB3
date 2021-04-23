//
// SiBi.cpp : Test only
//

#include <iostream>
#include <cmath>
#include <algorithm>
#include <chrono>

// From https://github.com/lucianpls/libicd
#include <icd_codecs.h>

#include "bmap.h"
#include "denc.h"

using namespace std;
using namespace chrono;
using namespace SiBi;
NS_ICD_USE

template<typename T>
vector<T> to(vector<uint8_t> &v, T m) {
    vector<T> result;
    result.reserve(v.size());
    for (auto it : v)
        result.push_back(m * it);
    return result;
}

template<typename T>
void check(vector<uint8_t> &image, const Raster &raster, uint64_t m) {
    size_t xsize = raster.size.x;
    size_t ysize = raster.size.y;
    size_t bands = raster.size.c;
    high_resolution_clock::time_point t1, t2;
    double time_span;

    auto img = to(image, static_cast<T>(m));
    t1 = high_resolution_clock::now();
    auto v = sincode(img, xsize, ysize, 0);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();

    //cout << "Encoded " << sizeof(T) * 8 << " size is " << v.size()
    //    << "\tCompressed to " << float(v.size()) * 100 / image.size() / sizeof(T)
    //    << "\tTook " << time_span << " seconds.";

    cout << float(v.size()) * 100 / image.size() / sizeof(T) << "\t" << time_span << "\t";


    t1 = high_resolution_clock::now();
    auto re = unsin<T>(v, xsize, ysize, bands, 0);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << time_span;

    if (img != re) {
        for (size_t i = 0; i < img.size(); i++)
            if (img[i] != re[i])
                cout << "Difference at " << i << " "
                << img[i] << " " << re[i] << endl;
    }
}

int test(string fname) {
	FILE* f;
	if (fopen_s(&f, fname.c_str(), "rb") || !f) {
		cerr << "Can't open input file\n";
		exit(errno);
	}
	fseek(f, 0, SEEK_END);
	auto fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	std::vector<uint8_t> src(fsize);
	storage_manager source = { src.data(), src.size() };
	fread(source.buffer, fsize, 1, f);
	fclose(f);
	Raster raster;
	auto error_message = image_peek(source, raster);
	if (error_message) {
		cerr << error_message << endl;
		exit(1);
	}
	//cout << "Size is " << raster.size.x << "x" << raster.size.y << "@" << raster.size.c << endl;

	codec_params params(raster);
	std::vector<uint8_t> image(params.get_buffer_size());
	stride_decode(params, source, image.data());

	check<uint8_t>(image, raster, 1);
    return 0;
}

int main(int argc, char **argv)
{
    bool test_bitmap = false;
    bool test_RQ3 = true;

    if (test_bitmap) {
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

        vector<uint8_t> values;
        Bitstream s(values);
        cout << endl << bm.dsize() * 8 << endl;
        bm.pack(s);
        cout << s.v.size() * 8 << endl;

        vector<uint8_t> v;
        RLE(values, v);
        cout << v.size() * 8 << endl;
        vector<uint8_t> outv;
        Bitstream outs(outv);
        unRLE(v, outs.v);
        cout << outs.v.size() * 8 << std::endl;
        if (memcmp(values.data(), outv.data(), outv.size()))
            cerr << "RLE error" << endl;

        BMap bm1(sx, sy);
        // Reset s for reading
        s.bitp = 0;
        bm1.unpack(outs);
        if (!bm1.compare(bm))
            cerr << "Bitmap packing error" << endl;
    }

    if (test_RQ3) {
        if (argc < 2) {
            string fname;
            for (;;) {
                getline(cin, fname);
                if (fname.empty())
                    break;
                cout << fname << '\t';
                test(fname);
                cout << endl;
            }
            return 0;
        }

        string fname = argv[1];
        FILE* f;
        if (fopen_s(&f, fname.c_str(), "rb") || !f) {
            cerr << "Can't open input file\n";
            exit(errno);
        }
        fseek(f, 0, SEEK_END);
        auto fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::vector<uint8_t> src(fsize);
        storage_manager source = { src.data(), src.size() };
        fread(source.buffer, fsize, 1, f);
        fclose(f);
        Raster raster;
        auto error_message = image_peek(source, raster);
        if (error_message) {
            cerr << error_message << endl;
            exit(1);
        }
        cout << "Size is " << raster.size.x << "x" << raster.size.y << "@" << raster.size.c << endl;

        codec_params params(raster);
        std::vector<uint8_t> image(params.get_buffer_size());
        stride_decode(params, source, image.data());

        // From here on, test the algorithm for different data types
        int bsize = 4;
        //check<uint64_t>(image, raster, 1ull << 56); 
        //cout << endl;
        //check<uint64_t>(image, raster, 5);
        //cout << endl;
        //check<uint32_t>(image, raster, 5);
        //cout << endl;
        //check<uint32_t>(image, raster, 1ull << 24);
        //cout << endl;
        //check<uint16_t>(image, raster, 5);
        //cout << endl;
        //check<uint16_t>(image, raster, 1ull << 8);
        //cout << endl;
        check<uint8_t>(image, raster, 1);
        cout << endl;
    }

    return 0;
}