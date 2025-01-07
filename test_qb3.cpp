/*
Content: QB3 algorithm testing

Copyright 2020-2024 Esri
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
#include <iomanip>

// From https://github.com/lucianpls/libicd
#include <icd_codecs.h>
#include "QB3lib/QB3.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <WinBase.h>
#include <processthreadsapi.h>
#endif

using namespace std;
using namespace chrono;
NS_ICD_USE

// multiply a vector by a constant, with type conversion
template<typename inT, typename outT>
vector<outT> to(vector<inT> &v, outT m) {
    vector<outT> result(v.size());
    auto out = result.data();
    for (auto it : v)
        *out++ = m * it;
    return result;
}

template<typename inT, typename outT>
vector<outT> toplus(vector<inT>& v, outT m) {
    vector<outT> result(v.size());
    auto out = result.data();
    for (auto it : v)
        *out++ = m + it;
    return result;
}

template<typename T>
void check(vector<uint8_t> &image, const Raster &raster,
    uint64_t m, int main_band = 0,
    bool fast = 0, uint64_t q = 1, bool away = false, bool ftl = false)
{
    size_t xsize = raster.size.x;
    size_t ysize = raster.size.y;
    size_t bands = raster.size.c;
    high_resolution_clock::time_point t1, t2;
    double time_span;

    auto img = to(image, static_cast<T>(m));
    qb3_dtype tp = qb3_dtype::QB3_U8;
    if (is_signed<T>()) {
        tp = sizeof(T) == 8 ? qb3_dtype::QB3_I64 : sizeof(T) == 4 ? qb3_dtype::QB3_I32 :
            sizeof(T) == 2 ? qb3_dtype::QB3_I16 : qb3_dtype::QB3_I8;
    }
    else {
        tp = sizeof(T) == 8 ? qb3_dtype::QB3_U64 : sizeof(T) == 4 ? qb3_dtype::QB3_U32 :
            sizeof(T) == 2 ? qb3_dtype::QB3_U16 : qb3_dtype::QB3_U8;
    }
    auto qenc = qb3_create_encoder(xsize, ysize, bands, tp);
    vector<uint8_t> outvec(qb3_max_encoded_size(qenc));

    if (main_band != 1) {
        std::vector<size_t> cbands(bands);
        if (-1 == main_band)
            for (size_t i = 0; i < bands; i++)
                cbands[i] = i;
        qb3_set_encoder_coreband(qenc, bands, cbands.data());
    }

    // This is sufficient to trigger the quanta encoding
    if (q > 1)
        qb3_set_encoder_quanta(qenc, q, away);
    qb3_set_encoder_mode(qenc, ftl? qb3_mode::QB3M_FTL : fast ? qb3_mode::QB3M_BASE : qb3_mode::QB3M_BEST);

    t1 = high_resolution_clock::now();
    auto outsize = qb3_encode(qenc, static_cast<void *>(img.data()), outvec.data());
    time_span = duration_cast<duration<double>>(high_resolution_clock::now() - t1).count();
    auto err = qb3_get_encoder_state(qenc);
    qb3_destroy_encoder(qenc);
    qenc = nullptr;

    cout << std::setprecision(5) << outsize << '\t' << outsize * 100.0 / image.size() / sizeof(T) 
        << '\t' << image.size() * sizeof(T) / time_span / 1024 / 1024 
        << '\t' << time_span << '\t';
    if (err)
        cout << "\nFailed\n";

    std::vector<T> re(xsize * ysize * bands, 0);

    decsp qdec = nullptr;
    try {
        size_t image_size[3]; // Space for output values
        qdec = qb3_read_start(outvec.data(), outsize, image_size);
        if (!qdec)
            throw "Can't read formatted stream header";
        // Check the size
        if (xsize != image_size[0] || ysize != image_size[1] || bands != image_size[2])
            throw "Wrong decoded size";
        if (!qb3_read_info(qdec))
            throw "Reading QB3 headers failed";
        // Could query metadata here
        if (re.size() * sizeof(T) != qb3_decoded_size(qdec))
            throw "Wrong expected decoded size";
        t1 = high_resolution_clock::now();
        if (!qb3_read_data(qdec, re.data()))
            throw "Reading QB3 data failed";
        t2 = high_resolution_clock::now();
    }
    catch (const char* error) {
        cerr << "Error: " << error << endl;

    }
    qb3_destroy_decoder(qdec);

    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << sizeof(T) * image.size() /time_span / 1024 / 1024 << '\t'
        << time_span << '\t' << sizeof(T) << '\t' << m << '\t' << (ftl ? "FTL" : fast ? "Fast" : "");

    if (q > 1) {
        auto hq = T(q / 2); // precision
        for (size_t i = 0; i < img.size(); i++) {
            auto x = img[i];
            auto y = re[i];
            if (!((x == y) || ((x > y) && (y + hq >= x)) || ((y > x) && (x + hq >= y)))) {
                cout << endl << "Difference at " << i << " expect "
                    << hex << uint64_t(img[i]) << " != " << uint64_t(re[i]) << dec;
                cout << endl << "y = " << i / (xsize * bands) <<
                    " x = " << (i / bands) % xsize <<
                    " c = " << i % bands;
                break;
            }
        }
    }
    else {
        for (size_t i = 0; i < img.size(); i++)
            if (img[i] != re[i]) {
                cout << endl << "Difference at " << i << " expect "
                    << hex << uint64_t(img[i]) << " got " << uint64_t(re[i]) << dec;
                cout << endl << "y = " << i / (xsize * bands) <<
                    " x = " << (i / bands) % xsize <<
                    " c = " << i % bands;
                break;
            }
    }
}

// check stride decoding
template<typename T>
void check(vector<uint8_t>& image, const Raster& raster, size_t stride, bool fast = 0, uint64_t q = 0) {

    if (0 == stride)
        return check<T>(image, raster, 1, 0, fast, 1, false);

    size_t xsize = raster.size.x;
    size_t ysize = raster.size.y;
    size_t bands = raster.size.c;
    high_resolution_clock::time_point t1, t2;
    double time_span;

    auto img = to(image, static_cast<T>(1));
    qb3_dtype tp(QB3_U8);
    if (is_signed<T>()) {
        tp = sizeof(T) == 8 ? qb3_dtype::QB3_I64 : sizeof(T) == 4 ? qb3_dtype::QB3_I32 :
            sizeof(T) == 2 ? qb3_dtype::QB3_I16 : qb3_dtype::QB3_I8;
    }
    else {
        tp = sizeof(T) == 8 ? qb3_dtype::QB3_U64 : sizeof(T) == 4 ? qb3_dtype::QB3_U32 :
            sizeof(T) == 2 ? qb3_dtype::QB3_U16 : qb3_dtype::QB3_U8;
    }

    auto qenc = qb3_create_encoder(xsize, ysize, bands, tp);
    vector<uint8_t> outvec(qb3_max_encoded_size(qenc));
    if (q > 1)
        qb3_set_encoder_quanta(qenc, q, 0);

    t1 = high_resolution_clock::now();
    auto outsize = qb3_encode(qenc, static_cast<void*>(img.data()), outvec.data());
    time_span = duration_cast<duration<double>>(high_resolution_clock::now() - t1).count();
    auto err = qb3_get_encoder_state(qenc);
    qb3_destroy_encoder(qenc);
    qenc = nullptr;

    cout << outsize << '\t' << outsize * 100.0 / image.size() / sizeof(T)
        << '\t' << image.size() * sizeof(T) / time_span / 1024 / 1024
        << '\t' << time_span << '\t';
    if (err)
        cout << "\nFailed\n";

    std::vector<T> re(stride * ysize, 0);
    decsp qdec = nullptr;
    try {
        size_t image_size[3]; // Space for output values
        qdec = qb3_read_start(outvec.data(), outsize, image_size);
        if (!qdec)
            throw "Can't read qb3 formatted stream header";
        // Check the size
        if (xsize != image_size[0] || ysize != image_size[1] || bands != image_size[2])
            throw "Wrong decoded size";
        if (!qb3_read_info(qdec))
            throw "Reading QB3 headers failed";
        qb3_set_decoder_stride(qdec, stride);

        t1 = high_resolution_clock::now();
        if (!qb3_read_data(qdec, re.data()))
            throw "Reading QB3 data failed";
        t2 = high_resolution_clock::now();
    }
    catch (const char* error) {
        cerr << "Error: " << error << endl;
    }
    qb3_destroy_decoder(qdec);

    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << sizeof(T) * image.size() / time_span / 1024 / 1024 << '\t'
        << time_span << '\t' << sizeof(T) << '\t' << 1 << '\t';
    if (fast)
        cout << "Fast";

    // verify that the stride was respected
    bool failed = false;
    auto hq = T(q / 2); // precision
    for (size_t y = 0; !failed && y < ysize; y++) {
        auto src = img.data() + xsize * bands * y;
        auto dst = re.data() + stride * y;
        for (size_t x = 0; x < xsize * bands; x++)
            if (q > 1) {
                auto s = src[x];
                auto d = dst[x];
                if (!((s == d) || ((s > d) && (d + hq >= s)) || ((s < d) && (s + hq >= d)))) {
                    cout << endl << "Difference at line " << y << " column " << x / bands << " c " << x % bands
                        << ", expect " << hex << uint64_t(s) << " != " << uint64_t(d) << dec;
                    failed = true;
                    break;
                }
            }
            else {
                if (src[x] != dst[x]) {
                    cout << endl << "Difference at line " << y << " column " << x / bands << " c " << x % bands
                        << ", expect " << hex << uint64_t(src[x]) << " got " << uint64_t(dst[x]) << dec;
                    failed = true;
                    break;
                }
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

    qb3_set_encoder_mode(qenc, fast ? qb3_mode::QB3M_BASE : qb3_mode::QB3M_BEST);

    t1 = high_resolution_clock::now();
    auto outsize = qb3_encode(qenc, img.data(), outvec.data());
    time_span = duration_cast<duration<double>>(high_resolution_clock::now() - t1).count();

    cout << outsize << '\t' << outsize * 100.0 / image.size() / sizeof(T)
        << '\t' << time_span << '\t';

    std::vector<T> re(xsize * ysize * bands);

    size_t image_size[3] = {}; // Space for output values
    auto failed = false;
    auto expected_size = xsize * ysize * bands * 2; // 16bit data
    size_t actual_size(0);
    t1 = high_resolution_clock::now();
    auto qdec = qb3_read_start(outvec.data(), outsize, image_size);
    if (!qdec) failed = true; 
    else if (!qb3_read_info(qdec)) failed = true;
    else {
        failed |= xsize != image_size[0] || ysize != image_size[1] || bands != image_size[2];
        if (!failed)
            actual_size = qb3_read_data(qdec, re.data());
    }
    t2 = high_resolution_clock::now();
    failed |= expected_size != actual_size;

    qb3_destroy_decoder(qdec);
    qdec = nullptr;
    if (failed) {
        cerr << "Error: " << "Decoding failed" << endl;
        return;
    }

    time_span = duration_cast<duration<double>>(t2 - t1).count();
    cout << time_span;

    cout << time_span << '\t' << sizeof(T) << '\t' << m << '\t';
    if (fast)
        cout << "Fast";

    if (img == re)
        return;
    for (size_t i = 0; i < img.size(); i++) if (img[i] != re[i]) {
        cout << endl << "Difference at " << i << " "
            << img[i] << " " << re[i];
        cout << endl << "y = " << i / (xsize * bands) <<
            " x = " << (i / bands) % xsize <<
            " c = " << i % bands;
        break;
    }
}

int test(string fname) {
    FILE* f = fopen(fname.c_str(), "rb");
    if (!f) {
        cerr << "Can't open input file\n";
        exit(errno);
    }
    fseek(f, 0, SEEK_END);
    auto fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> src(fsize);
    storage_manager source = { src.data(), src.size() };
    if (!fread(source.buffer, fsize, 1, f)) {
        cerr << "Can't read input file\n";
        fclose(f);
        exit(errno);
    }
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
    bool test_QB3 = true;

#if defined(_WIN32) || defined(_WIN64)
    // Get the thread id
    DWORD tid = GetCurrentThreadId();
    // Set the thread affinity to the first core
    HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
    // error checking
    if (thread == NULL)
        cerr << "Failed to get thread handle\n";
    else {
        DWORD_PTR mask = 3; // First core, either CPU, likely the worst choice
        // best core on my machine, up to 10% faster than first
        mask = 0x10000;
        // worst core on my machine
        //mask = 0x30;
        if (!SetThreadAffinityMask(thread, mask))
            cerr << "Failed to set thread affinity\n";
        CloseHandle(thread);
    }
#endif

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
        FILE* f = fopen(fname.c_str(), "rb");
        if (!f) {
            cerr << "Can't open input file\n";
            exit(errno);
        }
        fseek(f, 0, SEEK_END);
        auto fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::vector<uint8_t> src(fsize);
        storage_manager source = { src.data(), src.size() };
        if (!fread(source.buffer, fsize, 1, f)) {
            cerr << "Can't read input file\n";
            fclose(f);
            exit(errno);
        }
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
            auto err_message = stride_decode(params, source, image.data());
            auto time_span = duration_cast<duration<double>>(high_resolution_clock::now() - t).count();
            if (err_message) {
                cerr << err_message << endl;
                return 1;
            }
            cout << "Decode time " << time_span << " rate " << image.size() / time_span / 1024 / 1024 << " MB/s" << endl;

            cout << "Size\tRatio %\tEnc (MB/s)\t(s)\tDec (MB/s)\t(s)\tT_Size\n\n";

            //cout << "Stride ";
            //check<uint8_t>(image, raster, raster.size.x * raster.size.c + 10, true);
            //cout << endl << endl;

            //cout << "Stride ";
            //check<uint8_t>(image, raster, raster.size.x * raster.size.c + 11, true);
            //cout << endl << endl;

            //cout << "Stride ";
            //check<uint8_t>(image, raster, raster.size.x * raster.size.c + 90, false);
            //cout << endl << endl;

            //cout << "Stride ";
            //check<uint8_t>(image, raster, raster.size.x * raster.size.c + 91, false);
            //cout << endl << endl;

            //cout << "Stride ";
            //check<uint8_t>(image, raster, raster.size.x * raster.size.c + 92, false);
            //cout << endl << endl;

            // The sign really messes things up for normal images, because transitions through 128 are frequent
            // and become massive
            cout << 2 << "q  ";
            check<uint8_t>(image, raster, 1, 1, true, 2);
            cout << endl;

            //// Hardly any difference from the one above from rounding away from 0
            cout << 2 << "q- ";
            check<uint8_t>(image, raster, 1, 1, true, 2, true);
            cout << endl;

            cout << 3 << "q  ";
            check<uint8_t>(image, raster, 1, 1, true, 3);
            cout << endl;

            // Same exact as above, for odd quanta there is no difference
            cout << 3 << "q- ";
            check<uint8_t>(image, raster, 1, 1, true, 3, true);
            cout << endl;

            // check stride, any value larger than sizex * sizec, same size as above
            cout << "Stride and quanta \n3q  ";
            check<uint8_t>(image, raster, raster.size.x* raster.size.c + 9, true, 3);
            cout << endl;

            cout << 4 << "q  ";
            check<uint8_t>(image, raster, 1, 1, true, 4);
            cout << endl;

            cout << 4 << "q- ";
            check<uint8_t>(image, raster, 1, 1, true, 4, true);
            cout << endl;

            cout << 10 << "q  ";
            check<uint8_t>(image, raster, 1, 1, true, 10);
            cout << endl;

            cout << 10 << "qb ";
            check<uint8_t>(image, raster, 1, 1, false, 10);
            cout << endl;
            cout << endl;

            check<uint64_t>(image, raster, 5, 1);
            cout << endl;
            check<uint64_t>(image, raster, (1ull << 56), 1);
            cout << endl;
            check<uint32_t>(image, raster, 5, 1);
            cout << endl;
            check<uint32_t>(image, raster, 1ull << 24, 1);
            cout << endl;
            check<uint16_t>(image, raster, 5, 1);
            cout << endl;
            check<uint16_t>(image, raster, 1ull << 8, 1);
            cout << endl;

            cout << "\nLarge rung\n";
            check<uint64_t>(image, raster, (1ull << 56), 1, true);
            cout << endl;
            check<uint32_t>(image, raster, 1ull << 24, 1, true);
            cout << endl;
            check<uint16_t>(image, raster, 1ull << 8, 1, true);
            cout << endl;

            cout << "\nData type\n";
            check<uint64_t>(image, raster, 1, 1);
            cout << endl;
            check<uint64_t>(image, raster, 1, 1, true);
            cout << endl;
            check<uint64_t>(image, raster, 1, 1, true, 1, 0, true);
            cout << endl;

            check<uint32_t>(image, raster, 1, 1);
            cout << endl;
            check<uint32_t>(image, raster, 1, 1, true);
            cout << endl;
            check<uint32_t>(image, raster, 1, 1, true, 1, 0, true);
            cout << endl;

            check<uint16_t>(image, raster, 1, 1);
            cout << endl;
            check<uint16_t>(image, raster, 1, 1, true);
            cout << endl;
            check<uint16_t>(image, raster, 1, 1, true, 1, 0, true);
            cout << endl;

            check<uint8_t>(image, raster, 1, 1);
            cout << endl;
            check<uint8_t>(image, raster, 1, 1, true);
            cout << endl;
            check<uint8_t>(image, raster, 1, 1, true, 1, 0, true);
            cout << endl;
        }
        else if (raster.dt == ICDT_Int16 || raster.dt == ICDT_UInt16) {
            std::vector<uint16_t> image(params.get_buffer_size() / 2);
            auto t = high_resolution_clock::now();
            auto err_message = stride_decode(params, source, image.data());
            auto time_span = duration_cast<duration<double>>(high_resolution_clock::now() - t).count();
            if (err_message) {
                cerr << err_message << endl;
                return 1;
            }

            cout << "Decode time " << time_span << endl;

            cout << "Compressed\tRatio\tEncode\tDecode\tType\n\n";
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