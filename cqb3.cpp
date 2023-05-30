/*

Basic QB3 image encode, uses libicd for input

Copyright 2023 Esri
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

#include <cstdint>
#include <iostream>
//#include <cmath>
//#include <algorithm>
#include <chrono>
#include <string>
#include <vector>
//#include <cassert>
//#include <type_traits>

// From https://github.com/lucianpls/libicd
#include <icd_codecs.h>
#include "QB3lib/QB3.h"

using namespace std;
using namespace chrono;
using namespace ICD;

struct options {
    options() : best(false), verbose(false) {};
    string in_fname;
    string out_fname;
    string error;
    bool best;
    bool verbose;
};

int Usage(const options &opt) {
    cerr << opt.error << endl << endl
        << "cqb3 [options] <input_filename> <output_filename>\n"
        << "Options:\n"
        << "\t-v : verbose\n"
        << "\t-b : best compression\n"
        ;    
    return 1;
}

bool parse_args(int argc, char** argv, options& opt) {
    for (int i = 1; i < argc; i++) {
        if (strlen(argv[i]) > 1 && argv[i][0] == '-') {
            // Option
            switch (argv[i][1]) {
            case 'v':
                opt.verbose = true;
                break;
            case 'b':
                opt.best = true;
                break;
            default:
                opt.error = "Uknown option provided";
                return false;
            }
        }
        else { // positional args
            // Could be just a slash
            string val(argv[i]);
            if (val.size() == 1) {
                opt.error = "Pipe operation not yet supported";
                return false;
            }

            // file name
            if (opt.in_fname.empty())
                opt.in_fname = val;
            else if (opt.in_fname.empty())
                opt.out_fname = val;
            else { // Too many positional args
                opt.error = "Too many positional arguments provided";
                return false;
            }
        }
    }

    // Adjust params
    if (opt.in_fname.empty()) {
        opt.error = "Need at least the input file name";
        return false;
    }
    // If output file name is not provided, extract from input file name
    if (opt.out_fname.empty()) {
        string fname(opt.in_fname);
        // Strip input path, write output in current folder
        if (fname.find_last_of("\\/") != string::npos)
            fname = fname.substr(fname.find_last_of("\\/") + 1);
        // Strip input extension
        fname = fname.substr(0, fname.find_first_of("."));
        opt.out_fname = fname + ".qb3";
    }
    // If the output name is a folder, append a derived fname
    if (opt.out_fname.find_last_of("\\/") == opt.out_fname.size()) {
        string fname(opt.in_fname);
        // Strip input path
        if (fname.find_last_of("\\/") != string::npos)
            fname = fname.substr(fname.find_last_of("\\/") + 1);
        // Strip extension
        fname = fname.substr(0, fname.find_first_of("."));
        opt.out_fname += fname + ".qb3";
    }
    return true;
}

int main(int argc, char** argv)
{
    options opts;
    if (!parse_args(argc, argv, opts))
        return Usage(opts);

    string fname = opts.in_fname;
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
    fread(source.buffer, fsize, 1, f);
    fclose(f);
    Raster raster;
    auto error_message = image_peek(source, raster);
    if (error_message) {
        cerr << error_message << endl;
        exit(1);
    }
    if (opts.verbose)
        cerr << "Size " << fsize << ", Image " << raster.size.x << "x" << raster.size.y << "@" << raster.size.c << endl;
    codec_params params(raster);

    if (raster.dt != ICDT_Byte) {
        cerr << "Only conversion from byte data implemented\n";
        return 2;
    }
    std::vector<uint8_t> image(params.get_buffer_size());
    auto t = high_resolution_clock::now();
    stride_decode(params, source, image.data());
    auto time_span = duration_cast<duration<double>>(high_resolution_clock::now() - t).count();
    if (opts.verbose)
        cerr << "Decode time: " << time_span << " rate: " 
            << image.size() / time_span / 1024 / 1024 << " MB/s" << endl;

    size_t xsize = raster.size.x;
    size_t ysize = raster.size.y;
    size_t bands = raster.size.c;
    high_resolution_clock::time_point t1, t2;

    auto qenc = qb3_create_encoder(xsize, ysize, bands, QB3_U8);
    vector<uint8_t> outvec(qb3_max_encoded_size(qenc));
    for (auto& v : outvec)
        v = 0;
    qb3_set_encoder_mode(qenc, opts.best ? qb3_mode::QB3M_BEST : qb3_mode::QB3M_BASE);
    t1 = high_resolution_clock::now();
    auto outsize = qb3_encode(qenc, static_cast<void*>(image.data()), outvec.data());
    t2 = high_resolution_clock::now();
    time_span = duration_cast<duration<double>>(t2 - t1).count();

    if (opts.verbose)
        cerr << "Output\nSize: " << outsize << " Ratio " << outsize * 100.0 / fsize << "%, encode time : " 
            << time_span << " s, rate : " 
            << image.size() / time_span / 1024 /1024 << " MB/s\n";

    f = fopen(opts.out_fname.c_str(), "wb");
    if (!f) {
        cerr << "Can't open output file\n";
        exit(errno);
    }
    fwrite(outvec.data(), outsize, 1, f);
    fclose(f);
	return 0;
}