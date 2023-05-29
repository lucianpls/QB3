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

int Usage() {
    cerr << "cqb3 <input_filename> <output_filename>";
    return 1;
}

int main(int argc, char** argv)
{
    if (argc < 2)
        return Usage();

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
    fread(source.buffer, fsize, 1, f);
    fclose(f);
    Raster raster;
    auto error_message = image_peek(source, raster);
    if (error_message) {
        cerr << error_message << endl;
        exit(1);
    }
    cerr << "Size is " << raster.size.x << "x" << raster.size.y << "@" << raster.size.c << endl;
    codec_params params(raster);

    if (raster.dt == ICDT_Byte) {

        std::vector<uint8_t> image(params.get_buffer_size());
        auto t = high_resolution_clock::now();
        stride_decode(params, source, image.data());
        auto time_span = duration_cast<duration<double>>(high_resolution_clock::now() - t).count();
        cerr << "Decode time " << time_span << " rate " << image.size() / time_span / 1024 / 1024 << " MB/s" << endl;
    }

	return 0;
}