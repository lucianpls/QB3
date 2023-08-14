/*

Basic QB3 image encode and decode, uses libicd for input

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
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <string>
#include <vector>
#include <utility>

// From https://github.com/lucianpls/libicd
#include <icd_codecs.h>
#include "QB3lib/QB3.h"

using namespace std;
using namespace chrono;
using namespace ICD;

struct options {
    options() : 
        best(false),
        trim(false),
        verbose(false), 
        decode(false) 
    {};

    string in_fname;
    string out_fname;
    string error;
    string mapping; // band mapping, if provided
    bool best;
    bool trim; // Trim input to a multiple of 4x4 blocks
    bool verbose;
    bool decode;
};

int Usage(const options &opt) {
    cerr << opt.error << endl << endl
        << "cqb3 [options] <input_filename> <output_filename>\n"
        << "Options:\n"
        << "\t-v : verbose\n"
        << "\t-d : decode from QB3\n"
        << "\n"
        << "Compression only options:\n"
        << "\t-b : best compression\n"
        << "\t-t : trim input to multiple of 4x4 pixels\n"
        << "\t-m <b,b,b> : core band mapping\n"
        ;
    return 1;
}


// A bandlist contains only digits and commas
bool isbandmap(const string& s) {
    const string valid("01234567890,");
    for (auto c : s) 
        if (valid.find(c) == valid.npos)
            return false;
    return true;
}

bool parse_args(int argc, char** argv, options& opt) {
    // Look at the executable name, it could be decode
    string codename(argv[0]);
    for (auto& c : codename)
        c = tolower(c);
    if (codename.find("dqb3") != string::npos)
        opt.decode = true;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != 0) {
            // Option
            switch (argv[i][1]) {
            case 'v':
                opt.verbose = true;
                break;
            case 'b':
                opt.best = true;
                break;
            case 'd':
                opt.decode = true;
                break;
            case 't':
                opt.trim = true;
                break;
            case 'm':
                // The next parameter is a comma separated band list if it starts with a digit
                opt.mapping = "-"; // Disable mapping
                if (i < argc && isbandmap(argv[i + 1]))
                    opt.mapping = argv[++i];
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
            else if (opt.out_fname.empty())
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
        opt.out_fname = fname + (opt.decode ? ".png" : ".qb3");
    }

    // If the output name is a folder, append a derived fname
    if (opt.out_fname.find_last_of("\\/") == opt.out_fname.size()) {
        string fname(opt.in_fname);
        // Strip input path
        if (fname.find_last_of("\\/") != string::npos)
            fname = fname.substr(fname.find_last_of("\\/") + 1);
        // Strip extension
        fname = fname.substr(0, fname.find_first_of("."));
        opt.out_fname += fname + (opt.decode ? ".png" : ".qb3");
    }

    // Conversion direction dependent options
    if (opt.decode) {
        if (opt.trim || opt.best) {
            opt.error = "Invalid option for QB3 decoding\n";
            return Usage(opt);
        }
    }
    else {

    }
    return true;
}

int decode_main(options& opts) {
    string fname = opts.in_fname;
    FILE* f = fopen(fname.c_str(), "rb");
    if (!f) {
        cerr << "Can't open input file\n";
        exit(errno);
    }
    fseek(f, 0, SEEK_END);
    auto fsize = ftell(f);
    rewind(f);
    std::vector<uint8_t> src(fsize);
    fread(src.data(), fsize, 1, f);
    fclose(f);

    // Decode the qb3
    size_t image_size[3];
    auto qdec = qb3_read_start(src.data(), fsize, image_size);
    vector<uint8_t> raw;
    double time_span(0);

    if (!qdec) {
        cerr << "Input not recognized as a valid qb3 raster\n";
        return 2;
    }
    try {
        if (!qb3_read_info(qdec)) {
            opts.error = "Can't read qb3 file headers";
            throw 2;
        }
        if (opts.verbose) {
            cout << "Input:\nSize " << src.size() << " Image "
                << image_size[0] << "x" << image_size[1] << "@"
                << image_size[2] << endl;
        }
        raw.resize(qb3_decoded_size(qdec));
        auto t1 = high_resolution_clock::now();
        auto rbytes = qb3_read_data(qdec, raw.data());
        auto t2 = high_resolution_clock::now();
        time_span = duration_cast<duration<double>>(t2 - t1).count();
    }
    catch (const int err_code) {
        cerr << opts.error << endl;
        qb3_destroy_decoder(qdec);
        return err_code;
    }

    // Query metadata before getting rid of the decoder
    auto dt = qb3_get_type(qdec);
    qb3_destroy_decoder(qdec);
    if (opts.verbose) {
        cerr << "Decode time: " << time_span << " rate: "
            << raw.size() / time_span / 1024 / 1024 << " MB/s\n";
    }

    if (dt != QB3_U8 && dt != QB3_U16) {
        cerr << "Only 8 and 16 bit PNG supported as output";
        return 1;
    }

    // Convert to PNG using libicd
    Raster image;
    image.dt = ICD::ICDT_Byte;
    // This assumes little endian
    if (dt == QB3_U16)
        image.dt = ICD::ICDT_UInt16;
    image.size.x = image_size[0];
    image.size.y = image_size[1];
    image.size.c = image_size[2];
    image.size.l = 0;
    image.size.z = 1;
    
    png_params params(image);
    //params.compression_level = 9;

    storage_manager png_src(raw.data(), raw.size());
    vector<uint8_t> png_buffer(raw.size() + raw.size() / 10); // Pad by 10%
    storage_manager png_blob(png_buffer.data(), png_buffer.size());
    auto t1 = high_resolution_clock::now();
    auto err_message = png_encode(params, png_src, png_blob);
    time_span = duration_cast<duration<double>>(high_resolution_clock::now() - t1).count();
    if (opts.error.size()) {
        cerr << "PNG encoding failed: " << err_message << endl;
        return 2;
    }
    if (opts.verbose) {
        cerr << "Output PNG:\nSize " << png_blob.size 
            << " Ratio: " << 100.0 * png_blob.size / src.size() << "%\n"
            << "Encode time: " << time_span << " rate: "
            << raw.size() / time_span / 1024 / 1024 << " MB/s\n";
    }

    // Write the output file
    f = fopen(opts.out_fname.c_str(), "wb");
    if (!f) {
        cerr << "Can't open output file\n";
        exit(errno);
    }
    fwrite(png_blob.buffer, png_blob.size, 1, f);
    fclose(f);

    return 0;
}

// Trim raster to a multiple of 4x4
// Remove order is N-1,0,N-2, until size is 4*x
void trim(Raster& raster, vector<unsigned char> &buffer) {
    size_t xsize = raster.size.x;
    size_t ysize = raster.size.y;
    size_t bands = raster.size.c;
    if (0 == ((xsize % 4) | (ysize % 4)))
        return; // Only trim if necessary
    // Pixel size in bytes
    size_t psize = ICD::getTypeSize(raster.dt, bands);
    size_t xstart = (xsize % 4) > 1; // Trim first column
    size_t ystart = (ysize % 4) > 1; // Trim first row
    size_t xend = xsize - ((xsize % 4) > 0) - ((xsize % 4) > 2); // Last one or two columns
    size_t yend = ysize - ((ysize % 4) > 0) - ((ysize % 4) > 2); // Last one or two rows
    // Adjust output raster size
    raster.size.x = (raster.size.x / 4) * 4;
    raster.size.y = (raster.size.y / 4) * 4;
    // Smaller than the input
    std::vector<unsigned char> outbuffer;
    outbuffer.reserve(buffer.size());
    // copy one line at a time
    for (size_t y = ystart; y < yend; y++)
        outbuffer.insert(outbuffer.end(),
            &buffer[psize * (y * xsize + xstart)],
            &buffer[psize * (y * xsize + xend)]);
    // Return in buffer
    swap(buffer, outbuffer);
}

int encode_main(options& opts) {
    string fname = opts.in_fname;
    FILE* f = fopen(fname.c_str(), "rb");
    if (!f) {
        cerr << "Can't open input file\n";
        return errno;
    }
    fseek(f, 0, SEEK_END);
    auto fsize = ftell(f);
    rewind(f);
    std::vector<uint8_t> src(fsize);
    storage_manager source = { src.data(), src.size() };
    fread(source.buffer, fsize, 1, f);
    fclose(f);
    Raster raster;
    auto error_message = image_peek(source, raster);
    if (error_message) {
        cerr << error_message << endl;
        return 1;
    }

    if (opts.verbose)
        cout << "Image " << raster.size.x << "x" << raster.size.y << "@"
            << raster.size.c << "\nSize " << fsize
            << ((raster.dt != ICDT_Byte) ? " 16bit\n" : "\n");

    if (raster.size.x < 4 || raster.size.y < 4) {
        cerr << "QB3 requires input size to be a multiple of 4\n";
        return 2;
    }

    if (raster.dt != ICDT_Byte && raster.dt != ICDT_UInt16 && raster.dt != ICDT_Short) {
        cerr << "Only conversion from 8 and 16 bit data implemented\n";
        return 2;
    }

    codec_params params(raster);
    std::vector<uint8_t> image(params.get_buffer_size());
    auto t = high_resolution_clock::now();
    stride_decode(params, source, image.data());
    auto time_span = duration_cast<duration<double>>(high_resolution_clock::now() - t).count();

    if (opts.verbose)
        cerr << "Decode time: " << time_span << "s\nRatio " << fsize * 100.0 / image.size() << "%, rate: "
        << image.size() / time_span / 1024 / 1024 << " MB/s\n";

    if ((raster.size.x % 4 || raster.size.y % 4) && opts.trim) {
        trim(raster, image);
        if (opts.verbose)
            cerr << "Trimmed to " << raster.size.x << "x" << raster.size.y << endl;
    }

    size_t xsize = raster.size.x;
    size_t ysize = raster.size.y;
    size_t bands = raster.size.c;
    qb3_dtype dt = raster.dt == ICDT_Byte ? QB3_U8 : ICDT_UInt16 ? QB3_U16 : QB3_I16;

    auto qenc = qb3_create_encoder(xsize, ysize, bands, dt);
    vector<uint8_t> outvec(qb3_max_encoded_size(qenc), 0);
    size_t outsize(0);
    if (!opts.mapping.empty()) {
        // Modify band mapping
        size_t bmap[QB3_MAXBANDS];
        if (opts.mapping == "-") {
            for (int i = 0; i < bands; i++)
                bmap[i] = i;
        }
        else {
            string mapping(opts.mapping);
            for (int i = 0; i < bands; i++) {
                if (mapping.empty()) {
                    bmap[i] = i; // identity
                    continue;
                }
                char* end(nullptr);
                bmap[i] = strtoul(mapping.c_str(), &end, 10);
                while (',' == *end) end++; // Skip commas
                mapping = end; // The unparsed part
            }
        }
        auto success = qb3_set_encoder_coreband(qenc, bands, bmap);
        if (!success)
            cerr << "Invalid band mapping, adjusted\n";
    }
    try {
        high_resolution_clock::time_point t1, t2;
        qb3_set_encoder_mode(qenc, opts.best ? qb3_mode::QB3M_BEST : qb3_mode::QB3M_BASE);
        t1 = high_resolution_clock::now();
        outsize = qb3_encode(qenc, static_cast<void*>(image.data()), outvec.data());
        t2 = high_resolution_clock::now();
        time_span = duration_cast<duration<double>>(t2 - t1).count();
    }
    catch (int err_code) {
        cerr << opts.error << endl;
        qb3_destroy_encoder(qenc);
        return err_code;
    }
    qb3_destroy_encoder(qenc);

    if (opts.verbose) {
        cerr << "Output\nSize: " << outsize << "\nEncode time : " << time_span << "s\nRatio "
            << outsize * 100.0 / image.size() << "%, encode rate : "
            << image.size() / time_span / 1024 / 1024 << " MB/s\n";
        cerr << outsize * 100.0 / fsize << "% of the input\n";
    }

    f = fopen(opts.out_fname.c_str(), "wb");
    if (!f) {
        cerr << "Can't open output file\n";
        exit(errno);
    }
    fwrite(outvec.data(), outsize, 1, f);
    fclose(f);
    return 0;
}

int main(int argc, char** argv)
{
    options opts;
    if (!parse_args(argc, argv, opts))
        return Usage(opts);
    return opts.decode ? decode_main(opts) : encode_main(opts);
}
