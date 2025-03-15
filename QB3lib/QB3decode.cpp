/*
Content: C API QB3 decoding

Copyright 2021-2025 Esri
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

#pragma warning(disable:4127) // conditional expression is constant
#include "QB3decode.h"
// For memset, memcpy
#include <cstring>
#include <vector>

// Main QB3 file header
// 4 signature
// 2 xmax
// 2 ymax
// 1 bandmax
// 1 data type
// 1 mode
constexpr size_t QB3_HDRSZ = 4 + 2 + 2 + 1 + 1 + 1;

void qb3_destroy_decoder(decsp p) {
    delete p;
}

size_t qb3_decoded_size(const decsp p) {
    return p->xsize * p->ysize * p->nbands * szof(p->type);
}

qb3_dtype qb3_get_type(const decsp p) {
    return p->type;
}

qb3_mode qb3_get_mode(const decsp p) {
    return (2 == p->stage) ? p->mode : QB3M_INVALID;
}

uint64_t qb3_get_quanta(const decsp p) {
    return (2 == p->stage) ? p->quanta: 0;
}

uint64_t qb3_get_order(const decsp p) {
    if (p->stage != 2)
        return 0; // Error
    return p->order ? p->order : ZCURVE;
}

// Get a copy of the coreband mapping, as read from QB3 header
bool qb3_get_coreband(const decsp p, size_t *coreband) {
    if (p->stage != 2)
        return false; // Error
    for (int c = 0; c < p->nbands; c++)
        coreband[c] = p->cband[c];
    return true;
}

// Change the line to line stride, defaults to line size
void qb3_set_decoder_stride(decsp p, size_t stride) {
    p->stride = stride;
}

// Integer multiply but don't overflow, at least on the positive side
template<typename T>
static void dequantize(T* d, const decsp p) {
    size_t sz = qb3_decoded_size(p) / sizeof(T);
    const T q = static_cast<T>(p->quanta);
    const T mai = std::numeric_limits<T>::max() / q; // Top valid value
    const T mii = std::numeric_limits<T>::min() / q; // Bottom valid value
    auto stride = p->stride ? p->stride : (p->xsize * p->nbands * sizeof(T));
    // Slightly faster without a double loop
    if (stride == p->xsize * p->nbands * sizeof(T)) {
        for (size_t i = 0; i < sz; i++) {
            auto data = d[i];
            d[i] = (data <= mai) * (data * q)
                + (!(data <= mai)) * std::numeric_limits<T>::max();
            if (std::is_signed<T>() && (q > 2) && (data < mii))
                d[i] = std::numeric_limits<T>::min();
        }
        return;
    }
    // With line stride
    for (size_t y = 0; y < p->ysize; y++) {
        auto dst = reinterpret_cast<T*>(
                reinterpret_cast<uint8_t*>(d) + y * stride);
        for (size_t i = 0; i < p->nbands * p->xsize; i++) {
            auto data = dst[i];
            dst[i] = (data <= mai) * (data * q)
                + (!(data <= mai)) * std::numeric_limits<T>::max();
            if (std::is_signed<T>() && (q > 2) && (data < mii))
                dst[i] = std::numeric_limits<T>::min();
        }
    }
}

// Check a 2 byte signature
static bool check_sig(uint64_t val, const char *sig) {
    return (val & 0xff) == uint8_t(sig[0]) 
        && ((val >> 8) & 0xff) == uint8_t(sig[1]);
}

// Returns true if the 64bit value represents a valid curve
// This means that every nibble value has to be present, from 0 to F
static bool check_curve(uint64_t val) {
    int mask(0);
    // Each unique nibble sets one of the lower 16 bits
    for (int i = 0; i < 16; i++) {
        mask |= (1 << (val & 0xf));
        val >>= 4;
    }
    return mask == 0xffff;
}

// Starts reading a formatted QB3 source
// returns nullptr if it fails, usually because the source is corrupt
// If successful, size containts 3 values, x size, y size and number of bands
decsp qb3_read_start(void* source, size_t source_size, size_t *image_size) {
    if (source_size < QB3_HDRSZ + 4 || nullptr == image_size)
        return nullptr; // Too short to be a QB3 format stream
    iBits s(reinterpret_cast<uint8_t*>(source), source_size);
    auto val = s.pull(64);
    if (!check_sig(val, "QB") || !check_sig(val >> 16, "3\200"))
        return nullptr;
    auto p = new decs;
    memset(p, 0, sizeof(decs));
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
        || (p->mode >= qb3_mode::QB3M_END && p->mode != qb3_mode::QB3M_STORED)
        || 0 != (val & 0x8080) 
        || p->type > qb3_dtype::QB3_I64) {
        delete p;
        return nullptr;
    }
    p->s_in = static_cast<uint8_t*>(source) + QB3_HDRSZ;
    p->s_size = source_size - QB3_HDRSZ;

    // Pass back the image size
    image_size[0] = p->xsize;
    image_size[1] = p->ysize;
    image_size[2] = p->nbands;
    if (QB3M_BASE_Z == p->mode || QB3M_CF == p->mode 
        || QB3M_CF_RLE == p->mode || QB3M_RLE == p->mode)
        p->order = ZCURVE;
    p->error = QB3E_OK;
    p->stage = 1; // Read main header
    return p; // Looks reasonable
}

// read the rest of the qb3 stream metadata
// Returns true if no failure is detected and DT is found
bool qb3_read_info(decsp p) {
    // Check that the input structure is in the correct stage
    if (p->stage != 1 || p->error || !p->s_in || p->s_size < 4) {
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
            s.advance(32); // Skip the bytes we read
            p->quanta = s.pull(size_t(len) * 8);
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
            s.advance(32); // CHUNK + LEN
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
            if (p->s_size <= used) {
                p->error = QB3E_EINV;
                break;
            }
            p->s_in += used;
            p->s_size -= used;
            p->stage = 2; // Seen data header
        }
        // For SC, read the curve in p->order
        else if (check_sig(chunk, "SC")) {          
            // len should be 8
            if (len != 8) {
                p->error = QB3E_EINV;
                break;
            }
            // Misplaced curve chunk
            if (p->mode < qb3_mode::QB3M_BASE_H 
                || p->mode == qb3_mode::QB3M_STORED) {
                p->error = QB3E_EINV;
                break;
            }
            s.advance(32); // CHUNK + LEN
            p->order = s.pull(64);
            // Verify that the curve is valid
            if (!check_curve(p->order)) {
                p->error = QB3E_EINV;
                break;
            }
        }
        else {
            // Unknown chunk
            // Ignore it if the first letter is lower case
            if (chunk & 0x20)
                s.advance(size_t(len) * 8);
            // Otherwise, it's an error
            else
                p->error = QB3E_UNKN;
        }
    } while (p->stage != 2 && QB3E_OK == p->error && !s.empty());
    if (QB3E_OK == p->error && 2 != p->stage) // Should be s.empty()
        p->error = QB3E_EINV; // not expected
    return QB3E_OK == p->error;
}

// Decode RLE0 data, returns unused bytes, or negative if the dlen is too small
static int64_t deRLE0(const uint8_t* src, size_t slen, uint8_t* d, size_t dlen)
{
    const uint8_t* end = src + slen;
    const uint8_t* last = d + dlen;
    while ((d < last) && (src < end - 2)) {
        uint8_t c = *src++; // Get the next byte
        if (0xff != c || 0xff != src[0]) { // Not 2x ff
            *d++ = c;
            continue;
        }
        size_t count = 2; // Bytes to output
        if (0xff != src[1]) {
            c = 0;
            count = 4 + src[1]; // 4 to 258 zeros
        }
        if (last - d < count)
            return d - last; // Not enough room
        src += 2;
        while (count--)
            *d++ = c;
    }
    while (src < end && d < last)
        *d++ = *src++;
    return int64_t(last - d) - int64_t(end - src);
}

// The size of the decoded data
static size_t deRLE0Size(const uint8_t* src, size_t len)
{
    const uint8_t* end = src + len;
    size_t count(0);
    while (src < end - 2) { // A run is three bytes
        if (0xff != src[0] || 0xff != src[1]) {
            count++;
            src++;
            continue;
        }
        count += ((0xff == src[2]) ? 2 : 4ull + src[2]);
        src += 3;
    }
    return count + end - src;
}


static bool needs_rle(qb3_mode mode) {
    return (QB3M_RLE == mode || QB3M_RLE_H == mode 
        || QB3M_CF_RLE == mode || QB3M_CF_RLE_H == mode);
}

// returns 0 if an error is detected
// TODO: Error reporting
// source points to data to decode
static size_t qb3_decode(decsp p, void* source, size_t src_sz, void* dst)
{
    int error_code = 0;
    auto src = reinterpret_cast<uint8_t *>(source);
    // If the data is stored and size is right, just copy it
    if (p->mode == qb3_mode::QB3M_STORED) {
        // Only if the size is what we expect
        if (src_sz != qb3_decoded_size(p)) {
            p->error = QB3E_EINV;
            return 0;
        }
        memcpy(dst, source, src_sz);
        return src_sz;
    }
    std::vector<uint8_t> buffer;
    // If RLE is needed, it is expensive, allocates a whole new buffer
    if (needs_rle(p->mode)) {
        // RLE needs to be decoded into a temporary buffer
        auto sz = deRLE0Size(src, src_sz);
        // Abort if the size is larger than the raw data
        // This is a sanity check, preventing malicious input
        if (sz > qb3_decoded_size(p)) {
            p->error = QB3E_ERR;
            return 0;
        }
        buffer.resize(sz);
        auto err = deRLE0(src, src_sz, buffer.data(), sz);
        if (err != 0) {
            p->error = QB3E_EINV;
            return 0;
        }
        // Retarget the source to the buffer
        src = buffer.data();
        src_sz = sz;
    }

#define DEC(T) QB3::decode(src, src_sz, reinterpret_cast<T*>(dst), *p)
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

#define MUL(T) dequantize(reinterpret_cast<T *>(dst), p)
    // We have a quanta, decode in place
    if (!error_code && p->quanta > 1) {
        switch (p->type) {
        case qb3_dtype::QB3_U8:  MUL(uint8_t);  break;
        case qb3_dtype::QB3_I8:  MUL(int8_t);   break;
        case qb3_dtype::QB3_U16: MUL(uint16_t); break;
        case qb3_dtype::QB3_I16: MUL(int16_t);  break;
        case qb3_dtype::QB3_U32: MUL(uint32_t); break;
        case qb3_dtype::QB3_I32: MUL(int32_t);  break;
        case qb3_dtype::QB3_U64: MUL(uint64_t); break;
        case qb3_dtype::QB3_I64: MUL(int64_t);  break;
        default:
            error_code = 3; // Invalid type
        } // data type
    }
#undef MUL
    return error_code ? 0 : qb3_decoded_size(p);
}

// Call after read_header to read the actual data
size_t qb3_read_data(decsp p, void* dst) {
    // Check that it was a QB3 file
    if (p->stage != 2 || p->error != QB3E_OK
        || p->s_in == nullptr || p->s_size == 0) {
        if (p->error == QB3E_OK)
            p->error = QB3E_EINV;
        return 0; // Error signal
    }
    return qb3_decode(p, p->s_in, p->s_size, dst);
}
