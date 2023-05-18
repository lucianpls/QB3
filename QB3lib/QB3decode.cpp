/*
Copyright 2021-2022 Esri
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Content: C API QB3 decoding

Contributors:  Lucian Plesea
*/

#include "QB3common.h"
#include "QB3decode.h"
// For memset
#include <cstring>

// Builds a decoder control structure for raw qb3 input
decsp qb3_create_raw_decoder(size_t width, size_t height, size_t bands, qb3_dtype dt) {
    if (width > 0x10000ul || height > 0x10000ul || bands == 0 || bands > QB3_MAXBANDS)
        return nullptr;
    auto p = new decs;
    p->xsize = width;
    p->ysize = height;
    p->nbands = bands;
    p->type = dt;
    p->quanta = 0;
    p->raw = true;
    // Start with no inter-band differential
    for (size_t c = 0; c < bands; c++)
        p->cband[c] = c;
    // For 3 or 4 bands we assume RGB(A) input and use R-G and B-G
    if (bands == 3 || bands == 4)
        p->cband[0] = p->cband[2] = 1;
    return p;
}

void qb3_destroy_decoder(decsp p) {
    delete p;
}

bool qb3_set_decoder_coreband(decsp p, size_t bands, const size_t* cband) {
    if (bands != p->nbands)
        return false; // Incorrect band number
    // Store the new mapping
    for (size_t i = 0; i < bands; i++)
        p->cband[i] = (cband[i] < bands) ? cband[i] : i;
    return true;
}

bool qb3_set_decoder_quanta(decsp p, size_t q) {
    p->quanta = 1;
    if (q < 1) // Quanta of zero if not valid
        return false;
    p->quanta = q;
    if (q == 1) // No quantization
        return true;
    // Check the quanta value agains the max positive by type
    bool error = false;
    switch (p->type) {
#define TOO_LARGE(Q, T) (Q > uint64_t(std::numeric_limits<int8_t>::max()))
    case qb3_dtype::QB3_I8:
        error |= TOO_LARGE(p->quanta, int8_t);
    case qb3_dtype::QB3_U8:
        error |= TOO_LARGE(p->quanta, uint8_t);
    case qb3_dtype::QB3_I16:
        error |= TOO_LARGE(p->quanta, int16_t);
    case qb3_dtype::QB3_U16:
        error |= TOO_LARGE(p->quanta, uint16_t);
    case qb3_dtype::QB3_I32:
        error |= TOO_LARGE(p->quanta, int32_t);
    case qb3_dtype::QB3_U32:
        error |= TOO_LARGE(p->quanta, uint32_t);
    case qb3_dtype::QB3_I64:
        error |= TOO_LARGE(p->quanta, int64_t);
    } // data type
#undef TOO_LARGE
    if (error)
        p->quanta = 1;
    return !error;
}

size_t qb3_decoded_size(const decsp p) {
    return p->xsize * p->ysize * p->nbands * typesizes[static_cast<int>(p->type)];
}

// Integer multiply but don't overflow, at least on the positive side
template<typename T>
static void dequantize(T* d, const decsp p) {
    size_t sz = qb3_decoded_size(p) / sizeof(T);
    const T q = static_cast<T>(p->quanta);
    const T mai = std::numeric_limits<T>::max() / q; // Top valid value
    const T mii = std::numeric_limits<T>::min() / q; // Bottom valid value
    for (size_t i = 0; i < sz; i++) {
        auto data = d[i];
        d[i] = (data <= mai) * (data * q) 
            + (!(data <= mai)) * std::numeric_limits<T>::max();
        if (std::is_signed<T>() && q > 2 && (data < mii))
            d[i] = std::numeric_limits<T>::min();
    }
}

// Check a 4 byte signature, in the lower 316its of val
static bool check_sig(uint64_t val, const char *sig) {
    uint8_t c0 = static_cast<uint8_t>(sig[0]);
    uint8_t c1 = static_cast<uint8_t>(sig[1]);
    return (val & 0xff) == c0 && ((val >> 8) & 0xff) == c1;
}

// Starts reading a formatted QB3 source
// returns nullptr if it fails, usually because the source is not in the correct format
// If successful, size containts 3 values, x size, y size and number of bands
decsp qb3_read_start(void* source, size_t source_size, size_t *image_size) {
    if (source_size < QB3_HDRSZ + 4 || nullptr == image_size)
        return nullptr; // Too short to be a QB3 format stream
    auto p = new decs;
    memset(p, 0, sizeof(decs));
    iBits s(reinterpret_cast<uint8_t*>(source), source_size);
    auto val = s.pull(64);
    if (!check_sig(val, "QB") || !check_sig(val >> 16, "3\200")) {
        delete p;
        return nullptr;
    }
    val >>= 32;
    p->xsize = 1 + (val & 0xffff);
    val >>= 16;
    p->ysize = 1 + (val & 0xffff);
    val = s.peek();
    p->nbands = 1 + (val & 0xff);
    val >>= 8; // Got 56 bits left
    p->type = qb3_dtype(val & 0xff);
    val >>= 8; // 48 bits left
    // Compression mode
    p->mode = static_cast<qb3_mode>(val & 0xff);
    val >>= 8; // 40 bits left
    // Also check that the next 2 bytes are a signature
    if (p->nbands > QB3_MAXBANDS 
        || p->mode > qb3_mode::QB3M_BEST
        || 0 != (val & 0x8080) 
        || p->type > qb3_dtype::QB3_I64) {
        delete p;
        return nullptr;
    }
    p->raw = false;
    p->s_in = static_cast<uint8_t*>(source) + QB3_HDRSZ;
    p->s_size = source_size - QB3_HDRSZ;

    // Pass back the image size
    image_size[0] = p->xsize;
    image_size[1] = p->ysize;
    image_size[2] = p->nbands;

    p->error = QB3E_OK;
    p->state = 1; // Read main header
    return p; // Looks reasonable
}

// read the rest of the qb3 stream metadata
// Returns true if no failure is detected and (first) IDAT is found
bool qb3_read_info(decsp p) {
    // Check that the input structure is in the correct state
    if (p->state != 1 || p->error || p->raw || !p->s_in || p->s_size < 4) {
        if (QB3E_OK == p->error)
            p->error = QB3E_EINV;
        return false; // Didn't work
    }

    iBits s(p->s_in, p->s_size);
    // Need to parse the headers
    do {
        auto val = s.peek();
        // Chunks are fixed 16bit values, low endian
        auto chunk = val & 0xffff;
        val >>= 16; // leftover bits
        // size of chunk, if available
        uint16_t len = uint16_t(val & 0xffffu);
        if (check_sig(chunk, "QV")) { // Quanta
            if (len > 4 || len < 1) {
                p->error = QB3E_EINV;
                break;
            }
            s.advance(16 + 16); // Skip the bytes we read
            p->quanta = s.pull(len * 8);
            // Could check that the quanta is consistent with the data type
            if (p->quanta < 2)
                p->error = QB3E_EINV;
        }
        else if (check_sig(chunk, "CB")) { // Core bands
            // check that is matches the band count
            if (len != p->nbands) {
                p->error = QB3E_EINV;
                break;
            }
            s.advance(16 + 16); // CHUNK + LEN
            for (size_t i = 0; i < p->nbands; i++) {
                p->cband[i] = 0xff & s.pull(8);
                if (p->cband[i] >= p->nbands)
                    p->error = QB3E_EINV;
            }
            // Should we check the mapping?
        }
        else if (check_sig(chunk, "DT")) {
            s.advance(16);
            // Update the position
            size_t used = s.position() / 8;
            if (p->s_size > used) { // Should have some data
                p->s_in += used;
                p->s_size -= used;
                p->state = 2; // Seen data header
            }
        }
        else { // Unknown chunk
            p->error = QB3E_UNKN;
        }
    } while (p->state != 2 && QB3E_OK == p->error && !s.empty());
    if (QB3E_OK == p->error && 2 != p->state) // Should be s.empty()
        p->error = QB3E_EINV; // not expected
    return QB3E_OK == p->error;
}

// Call after read_header to read the actual data
size_t qb3_read_data(decsp p, void* destination) {
    // Check that it was a QB3 file
    if (p->state != 2 || p->error != QB3E_OK || p->raw 
        || p->s_in == nullptr || p->s_size == 0) {
        if (p->error == QB3E_OK)
            p->error = QB3E_EINV;
        return 0; // Error signal
    }
    return qb3_decode(p, p->s_in, p->s_size, destination);
}

// The encode public API, returns 0 if an error is detected
// TODO: Error reporting
size_t qb3_decode(decsp p, void* source, size_t src_sz, void* destination) {

    int error_code = 0;
    auto src = reinterpret_cast<uint8_t *>(source);

#define DEC(T) QB3::decode(src, src_sz, reinterpret_cast<T*>(destination), \
    p->xsize, p->ysize, p->nbands, p->cband)

    switch (p->type) {
    case qb3_dtype::QB3_U8:
    case qb3_dtype::QB3_I8:
        error_code = DEC(uint8_t); break;
    case qb3_dtype::QB3_U16:
    case qb3_dtype::QB3_I16:
        error_code = DEC(uint16_t); break;
    case qb3_dtype::QB3_U32:
    case qb3_dtype::QB3_I32:
        error_code = DEC(uint32_t); break;
    case qb3_dtype::QB3_U64:
    case qb3_dtype::QB3_I64:
        error_code = DEC(uint64_t); break;
    default:
        error_code = 3; // Invalid type
    } // data type
#undef DEC

#define MUL(T) dequantize(reinterpret_cast<T *>(destination), p)
    // We have a quanta, decode in place
    if (!error_code && p->quanta > 1) {
        switch (p->type) {
        case qb3_dtype::QB3_I8:
            MUL(int8_t); break;
        case qb3_dtype::QB3_U8:
            MUL(uint8_t); break;
        case qb3_dtype::QB3_I16:
            MUL(int16_t); break;
        case qb3_dtype::QB3_U16:
            MUL(uint16_t); break;
        case qb3_dtype::QB3_I32:
            MUL(int32_t); break;
        case qb3_dtype::QB3_U32:
            MUL(uint32_t); break;
        case qb3_dtype::QB3_I64:
            MUL(int64_t); break;
        case qb3_dtype::QB3_U64:
            MUL(uint64_t); break;
        default:
            error_code = 3; // Invalid type
        } // data type

    }
#undef MUL
    return error_code ? 0 : qb3_decoded_size(p);
}
