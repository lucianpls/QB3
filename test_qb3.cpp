/*
Copyright 2020-2021 Esri
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Contributors:  Lucian Plesea
*/

#include <iostream>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <string>
#include <vector>
#include <cassert>
#include <type_traits>

// From https://github.com/lucianpls/libicd
#include <icd_codecs.h>
#include <QB3.h>

using namespace std;
using namespace chrono;
NS_ICD_USE

template<typename inT, typename outT>
vector<outT> to(vector<inT> &v, outT m) {
    vector<outT> result;
    result.reserve(v.size());
    for (auto it : v)
        result.push_back(m * it);
    return result;
}

template<typename T>
void check(vector<uint8_t> &image, const Raster &raster, uint64_t m, int main_band = 0, bool fast = 0) {
    size_t xsize = raster.size.x;
    size_t ysize = raster.size.y;
    size_t bands = raster.size.c;
    high_resolution_clock::time_point t1, t2;
    double time_span;

    auto img = to(image, static_cast<T>(m));
    auto tp = sizeof(T) == 8 ? qb3_dtype::QB3_I64 : sizeof(T) == 4 ? qb3_dtype::QB3_I32 :
        sizeof(T) == 2 ? qb3_dtype::QB3_I16 : qb3_dtype::QB3_I8;
    auto qenc = qb3_create_encoder(xsize, ysize, bands, tp);
    vector<uint8_t> outvec(qb3_max_encoded_size(qenc));

    if (main_band != 1) {
        std::vector<size_t> cbands(bands);
        if (-1 == main_band)
            for (size_t i = 0; i < bands; i++)
                cbands[i] = i;
        qb3_set_encoder_coreband(qenc, bands, cbands.data());
    }
    t1 = high_resolution_clock::now();
    auto outsize = qb3_encode(qenc, static_cast<void *>(img.data()), outvec.data(), 
        fast ? qb3_mode::QB3_BASE : qb3_mode::QB3_BEST);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();
    //cout << "Encoded " << sizeof(T) * 8 << " size is " << v.size()
    //    << "\tCompressed to " << float(v.size()) * 100 / image.size() / sizeof(T)
    //    << "\tTook " << time_span << " seconds.";

    if (fast)
        cout << "Fast ";
    if (m != 1)
        cout << "Multiplier " << m;
    cout << " \tBPV " << sizeof(T) << '\t' << outsize << "\t"
        << outsize * 100.0 / image.size() / sizeof(T) << "\t" 
        << time_span << "\t";

    std::vector<T> re(xsize * ysize * bands);

    auto qdec = qb3_create_decoder(xsize, ysize, bands, tp);
    if (main_band != 1) {
        std::vector<size_t> cbands(bands);
        if (-1 == main_band)
            for (size_t i = 0; i < bands; i++)
                cbands[i] = i;
        qb3_set_decoder_coreband(qdec, bands, cbands.data());
    }
    t1 = high_resolution_clock::now();
    qb3_decode(qdec, outvec.data(), outsize, re.data());
    t2 = high_resolution_clock::now();

    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << time_span;

    if (img != re) {
        for (size_t i = 0; i < img.size(); i++)
            if (img[i] != re[i]) {
                cout << endl << "Difference at " << i << " "
                    << img[i] << " " << re[i];
                cout << endl << "y = " << i / (xsize * bands) <<
                    " x = " << (i / bands) % xsize <<
                    " c = " << i % bands;
                break;
            }
    }
}

template<typename T>
void check(vector<uint16_t>& image, const Raster& raster, uint64_t m, int main_band = 0, bool fast = 0) {
    size_t xsize = raster.size.x;
    size_t ysize = raster.size.y;
    size_t bands = raster.size.c;
    high_resolution_clock::time_point t1, t2;
    double time_span;

    auto img = to(image, static_cast<T>(m));
    auto tp = sizeof(T) == 8 ? qb3_dtype::QB3_I64 : sizeof(T) == 4 ? qb3_dtype::QB3_I32 :
        sizeof(T) == 2 ? qb3_dtype::QB3_I16 : qb3_dtype::QB3_I8;
    auto qenc = qb3_create_encoder(xsize, ysize, bands, tp);
    vector<uint8_t> outvec(qb3_max_encoded_size(qenc));

    //size_t bandmap[] = { 0, 1, 2, 3, 4, 5 };
    //qb3_set_encoder_coreband(qenc, 3, bandmap);
    t1 = high_resolution_clock::now();
    auto outsize = qb3_encode(qenc, img.data(), outvec.data(), fast ? qb3_mode::QB3_BASE : qb3_mode::QB3_BEST);
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();

    //cout << "Encoded " << sizeof(T) * 8 << " size is " << v.size()
    //    << "\tCompressed to " << float(v.size()) * 100 / image.size() / sizeof(T)
    //    << "\tTook " << time_span << " seconds.";

    if (fast)
        cout << "Fast ";
    if (m != 1)
        cout << "Multiplier " << m;
    cout << " \tBPV " << sizeof(T) << '\t' << outsize << "\t"
        << float(outsize) * 100 / image.size() / sizeof(T) << "\t"
        << time_span << "\t";
    std::vector<T> re(xsize * ysize * bands);

    auto qdec = qb3_create_decoder(xsize, ysize, bands, tp);
    //qb3_set_decoder_coreband(qdec, 3, bandmap);
    t1 = high_resolution_clock::now();
    qb3_decode(qdec, outvec.data(), outsize, re.data());
    t2 = high_resolution_clock::now();

    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << time_span;

    if (img != re) {
        for (size_t i = 0; i < img.size(); i++)
            if (img[i] != re[i]) {
                cout << endl << "Difference at " << i << " "
                    << img[i] << " " << re[i];
                cout << endl << "y = " << i / (xsize * bands) <<
                    " x = " << (i / bands) % xsize <<
                    " c = " << i % bands;
                break;
            }
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

    check<uint8_t>(image, raster, 1, 1);
    return 0;
}

int main(int argc, char **argv) 
{
    bool test_bitmap = false;
    bool test_QB3 = true;

    if (test_bitmap) {
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

        //vector<uint8_t> values((sx * sy + 7) / 8);
        //oBits s(values.data());
        //cout << endl << "Raw " << bm.dsize() * 8 << endl;
        //bm.pack(s);
        //cout << "Packed " << s.size_bits() << endl;

        //vector<uint8_t> v;
        //RLE(values, v);
        //cout << "RLE " << v.size() * 8 << endl;
        //vector<uint8_t> outv(sx * sy);
        //oBits outs(outv.data());
        //unRLE(v, outv);
        //cout << "UnRLE " << outs.size_bits() << std::endl;
        //if (memcmp(values.data(), outv.data(), outv.size()))
        //    cerr << "RLE error" << endl;

        //BMap bm1(sx, sy);
        //iBits ins(outv.data());
        //bm1.unpack(ins);
        //if (!bm1.compare(bm))
        //    cerr << "Bitmap packing error" << endl;
        //else
        //    cout << "Bitmap Success" << endl;
    }

    if (test_QB3) {
        if (argc < 2) {
            string fname;
            cout << "Provide input file name for testing QB3\n";
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
        // From here on, test the algorithm for different data types, with 
        // multiplied data so it covers most of the range
        if (raster.dt == ICDT_Byte) {

            std::vector<uint8_t> image(params.get_buffer_size());
            auto t = high_resolution_clock::now();
            stride_decode(params, source, image.data());
            auto time_span = duration_cast<duration<double>>(high_resolution_clock::now() - t).count();
            cout << "Decode time " << time_span << endl;

            cout << "Type" << '\t'
                << "Compressed" << "\t"
                << "Ratio" << "\t"
                << "Encode" << "\t"
                << "Decode" << "\t" << endl << endl;

            check<uint64_t>(image, raster, 5, 1);
            cout << endl;
            check<uint64_t>(image, raster, (1ull << 56) + 11, 1);
            cout << endl;
            check<uint32_t>(image, raster, 5, 1);
            cout << endl;
            check<uint32_t>(image, raster, 1ull << 24, 1);
            cout << endl;
            check<uint16_t>(image, raster, 5, 1);
            cout << endl;
            check<uint16_t>(image, raster, 1ull << 8, 1);
            cout << endl;

            cout << "Data type\n";
            check<uint64_t>(image, raster, 1, 1);
            cout << endl;
            check<uint32_t>(image, raster, 1, 1);
            cout << endl;
            check<uint16_t>(image, raster, 1, 1);
            cout << endl;
            check<uint16_t>(image, raster, 1, 1, true);
            cout << endl;
            check<uint8_t>(image, raster, 1, 1);
            cout << endl;
            check<uint8_t>(image, raster, 1, 1, true);
            cout << endl;
        }
        else if (raster.dt == ICDT_Int16 || raster.dt == ICDT_UInt16) {
            std::vector<uint16_t> image(params.get_buffer_size() / 2);
            auto t = high_resolution_clock::now();
            stride_decode(params, source, image.data());
            auto time_span = duration_cast<duration<double>>(high_resolution_clock::now() - t).count();
            cout << "Decode time " << time_span << endl;

            cout << "Type" << '\t'
                << "Compressed" << "\t"
                << "Ratio" << "\t"
                << "Encode" << "\t"
                << "Decode" << "\t" << endl << endl;
            check<uint16_t>(image, raster, 1, 1);
            cout << endl;
            check<uint16_t>(image, raster, 1, 1, true);
            cout << endl;
        }
        else {
            cerr << "Unsupported data type\n";
            return 1;
        }
    }

    return 0;
}