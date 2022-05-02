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

Content: C API QB3 encoding

Contributors:  Lucian Plesea
*/

#include "QB3common.h"
#include "QB3encode.h"
#include <limits>

// constructor
encsp qb3_create_encoder(size_t width, size_t height, size_t bands, int dt) {
    if (width > 0x10000ul || height > 0x10000ul || bands == 0 
        || bands > QB3_MAXBANDS || dt > int(QB3_I64))
        return nullptr;
    auto p = new encs;
    p->xsize = width;
    p->ysize = height;
    p->nbands = bands;
    p->type = static_cast<qb3_dtype>(dt);
    p->quanta = 1; // No quantization
    p->away = false; // Round to zero
    p->raw = false;  // Write image header
    p->mode = QB3M_DEFAULT; // Base
    // Start with no inter-band differential
    for (size_t c = 0; c < bands; c++) {
        p->runbits[c] = 0;
        p->prev[c] = 0;
        p->cband[c] = c;
    }
    // For 3 or 4 bands we assume RGB(A) input and use R-G and B-G
    if (bands == 3 || bands == 4)
        p->cband[0] = p->cband[2] = 1;
    p->error = 0;
    return p;
}

void qb3_destroy_encoder(encsp p) {
    delete p;
}

bool qb3_set_encoder_coreband(encsp p, size_t bands, size_t *cband) {
    if (bands != p->nbands)
        return false; // Incorrect band number
    // Set it, make sure it's not out of spec
    for (size_t i = 0; i < bands; i++)
        p->cband[i] = (cband[i] < bands) ? cband[i] : i;
    // Force core bands to be independent
    for (size_t i = 0; i < bands; i++)
        if (p->cband[i] != i)
            p->cband[p->cband[i]] = p->cband[i];
    // Return the possibly modified band mapping
    for (size_t i = 0; i < bands; i++)
        cband[i] = p->cband[i];
    return true;
}

// Sets quantization parameters
// Valid values are 2 and above
// sign = true when the input data is signed
// away = true to round away from zero
bool qb3_set_encoder_quanta(encsp p, size_t q, bool away) {
    p->quanta = 1;
    p->away = false;
    if (q < 1) // Quanta of zero if not valid
        return false;
    p->quanta = q;
    p->away = away;
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

// bytes per value by qb3_dtype, keep them in sync
const int typesizes[8] = { 1, 1, 2, 2, 4, 4, 8, 8 };

size_t qb3_max_encoded_size(const encsp p) {
    // Pad to 4 x 4
    size_t nvalues = 16 * ((p->xsize + 3) / 4) * ((p->ysize + 3) / 4) * p->nbands;
    // Maximum expansion is under 17/16 bits per input value, for large number of values
    double bits_per_value = 17.0 / 16.0 + typesizes[static_cast<int>(p->type)] * 8;
    return 1024 + static_cast<size_t>(bits_per_value * nvalues / 8);
}

qb3_mode qb3_set_encoder_mode(encsp p, qb3_mode mode) {
    if (mode <= qb3_mode::QB3M_BEST)
        p->mode = mode;
    return p->mode;
}

DLLEXPORT void qb3_set_encoder_raw(encsp p) {
    p->raw = true;
}

// Quantize in place then encode the source, by type
template<typename T>
bool quantize(T* source, oBits& s, encs& p) {
    size_t nV = p.xsize * p.ysize * p.nbands; // Number of values
    T q = static_cast<T>(p.quanta);
    if (q == 2) { // Easy to optimize for 2
        if (p.away)
            for (size_t i = 0; i < nV; i++)
                source[i] = source[i] / 2 + source[i] % 2;
        else
            for (size_t i = 0; i < nV; i++)
                source[i] /= 2;
        return 0;
    }

    if (p.away)
        for (size_t i = 0; i < nV; i++)
            source[i] = QB3::rfr0div(source[i], q);
    else
        for (size_t i = 0; i < nV; i++)
            source[i] = QB3::rto0div(source[i], q);
    return 0;
}

// A chunk signature is two characters
void static push_sig(const char* sig, oBits& s) {
    s.tobyte(); // Always at byte boundary
    s.push(*reinterpret_cast<const uint16_t *>(sig), 16);
}

// Main header, fixed size
// 
void static write_qb3_header(encsp p, oBits& s) {
    // QB3 signature is 4 bytes
    s.push(*reinterpret_cast<const uint32_t*>("QB3\200"), 32);
    // Always start at byte boundary
    if (p->xsize == 0 || p->ysize == 0
        || p->xsize > 0xffff || p->ysize > 0xffff
        || p->nbands > QB3_MAXBANDS
        || p->type > QB3_U64
        ) {
        p->error = QB3E_EINV;
        return;
    }

    // Write xmax, ymax, num bands in low endian
    s.push((p->xsize - 1), 16);
    s.push((p->ysize - 1), 16);
    s.push((p->nbands - 1), 8);
    s.push(static_cast<uint8_t>(p->type), 8); // all values are reserved
    s.push(static_cast<uint8_t>(p->mode), 8);  // Encoding style, all values are reserved
}

// Are there any band mappings
bool static is_banddiff(encsp p) {
    for (int c = 0; c < p->nbands; c++)
        if (p->cband[c] != c)
            return true;
    return false;
}

// Header for cbands, does nothing if not needed
void static write_cband_header(encsp p, oBits& s) {
    if (!is_banddiff(p)) // Is it needed
        return;
    push_sig("CB", s);
    s.push(p->nbands, 16); // size of payload
    // One byte per band, dump core band list
    for (int i = 0; i < p->nbands; i++)
        s.push(p->cband[i], 8); // 8 bits each
}

// Header for step, if used
void static  write_quanta_header(encsp p, oBits& s) {
    if (p->quanta < 2) // Is it needed
        return;
    push_sig("QV", s);
    size_t qbytes = 1 + topbit(p->quanta) / 8;
    s.push(qbytes, 16); // Payload bytes, 0 < v < 5
    s.push(p->quanta, qbytes);
}

// Data header has no knwon size
void static write_data_header(encsp p, oBits& s) {
    push_sig("DT", s);
}

void static write_headers(encsp p, oBits& s) {
    write_qb3_header(p, s);
    write_cband_header(p, s);
    write_quanta_header(p, s);
    write_data_header(p, s);
}

// Separate because it needs to operate on strips to avoid excessive memory consumption
// It might still not be sufficient
static size_t encode_quanta(encsp p, void* source, void* destination, qb3_mode mode) {
    oBits s(reinterpret_cast<uint8_t*>(destination));
    if (!p->raw) write_headers(p, s);
    if (p->error) return 0;

    encs subimg(*p);
    // Stripe at a time, in a temporary buffer that can be modified
    auto ysz(p->ysize);
    // In bytes
    auto linesize = p->xsize * p->nbands * typesizes[p->type];
    // Char can be type punned 
    std::vector<char> buffer(linesize * B);
    subimg.ysize = B;
    auto src = reinterpret_cast<char*>(source);

#define QENC(T) quantize(reinterpret_cast<T *>(buffer.data()), s, subimg);\
                if (mode == qb3_mode::QB3M_BEST) \
                    QB3::encode_best(reinterpret_cast<std::make_unsigned_t<T> *>(buffer.data()), s, subimg);\
                else\
                    QB3::encode_fast(reinterpret_cast<std::make_unsigned_t<T> *>(buffer.data()), s, subimg);

    for (size_t y = 0; y + B <= ysz; y += B) {
        memcpy(buffer.data(), src, buffer.size());
        switch (p->type) {
        case qb3_dtype::QB3_U8:  QENC(uint8_t);  break;
        case qb3_dtype::QB3_I8:  QENC(int8_t);   break;
        case qb3_dtype::QB3_U16: QENC(uint16_t); break;
        case qb3_dtype::QB3_I16: QENC(int16_t);  break;
        case qb3_dtype::QB3_U32: QENC(uint32_t); break;
        case qb3_dtype::QB3_I32: QENC(int32_t);  break;
        case qb3_dtype::QB3_U64: QENC(uint64_t); break;
        case qb3_dtype::QB3_I64: QENC(int64_t);  break;
        default: return QB3E_EINV;
        }
        src += linesize * B;
    }

    return (s.position() + 7) / 8;
#undef QENC
}


// The encode public API, returns 0 if an error is detected
size_t qb3_encode(encsp p, void* source, void* destination) {
    auto mode = p->mode;
    if (p->quanta > 1)
        return encode_quanta(p, source, destination, mode);
    oBits s(reinterpret_cast<uint8_t*>(destination));
    if (!p->raw) write_headers(p, s);
    if (p->error) return 0;

#define ENC(T) (mode == QB3M_BEST) ? \
              QB3::encode_best(reinterpret_cast<const T*>(source), s, *p)\
            : QB3::encode_fast(reinterpret_cast<const T*>(source), s, *p);

    switch (p->type) {
    case qb3_dtype::QB3_U8:
    case qb3_dtype::QB3_I8:
        p->error = ENC(uint8_t); break;
    case qb3_dtype::QB3_U16:
    case qb3_dtype::QB3_I16:
        p->error = ENC(uint16_t); break;
    case qb3_dtype::QB3_U32:
    case qb3_dtype::QB3_I32:
        p->error = ENC(uint32_t); break;
    case qb3_dtype::QB3_U64:
    case qb3_dtype::QB3_I64:
        p->error = ENC(uint64_t); break;
    default:
        p->error = QB3E_EINV; // Invalid type
    } // data type
#undef ENC
    return (p->error) ? 0 : s.tobyte();
}

int qb3_get_encoder_state(encsp p) {
    return p->error;
}