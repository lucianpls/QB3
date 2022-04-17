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
encsp qb3_create_encoder(size_t width, size_t height, size_t bands, qb3_dtype dt) {
    if (width > 0x10000ul || height > 0x10000ul || bands == 0 || bands > QB3_MAXBANDS)
        return nullptr;
    auto p = new encs;
    p->xsize = width;
    p->ysize = height;
    p->nbands = bands;
    p->type = dt;
    p->quanta = 1; // No quantization
    p->away = false; // Round to zero
    // Start with no inter-band differential
    for (size_t c = 0; c < bands; c++) {
        p->runbits[c] = 0;
        p->prev[c] = 0;
        p->cband[c] = c;
    }
    // For 3 or 4 bands we assume RGB(A) input and use R-G and B-G
    if (bands == 3 || bands == 4)
        p->cband[0] = p->cband[2] = 1;
    return p;
}

void qb3_destroy_encoder(encsp p) {
    delete p;
}

bool qb3_set_encoder_coreband(encsp p, size_t bands, const size_t *cband) {
    if (bands != p->nbands)
        return false; // Incorrect band number
    // Set it, make sure it's not out of spec
    for (size_t i = 0; i < bands; i++)
        p->cband[i] = (cband[i] < bands) ? cband[i] : i;
    // Force any core band to be independent
    for (size_t i = 0; i < bands; i++)
        if (p->cband[i] != i)
            p->cband[p->cband[i]] = p->cband[i];
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

static size_t encode_quanta(encsp p, void* source, void* destination, qb3_mode mode) {
    oBits s(reinterpret_cast<uint8_t*>(destination));
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
                if (mode == qb3_mode::QB3_BEST) \
                    QB3::encode_best(reinterpret_cast<std::make_unsigned_t<T> *>(buffer.data()), s, subimg);\
                else\
                    QB3::encode_fast(reinterpret_cast<std::make_unsigned_t<T> *>(buffer.data()), s, subimg);\
                break

    for (size_t y = 0; y + B <= ysz; y += B) {
        memcpy(buffer.data(), src, buffer.size());
        switch (p->type) {
        case qb3_dtype::QB3_U8:  QENC(uint8_t);
        case qb3_dtype::QB3_I8:  QENC(int8_t);
        case qb3_dtype::QB3_U16: QENC(uint16_t);
        case qb3_dtype::QB3_I16: QENC(int16_t);
        case qb3_dtype::QB3_U32: QENC(uint32_t);
        case qb3_dtype::QB3_I32: QENC(int32_t);
        case qb3_dtype::QB3_U64: QENC(uint64_t);
        case qb3_dtype::QB3_I64: QENC(int64_t);
        default: return 3;
        }
        src += linesize * B;
    }

    return (s.size_bits() + 7) / 8;
#undef QENC
}

// The encode public API, returns 0 if an error is detected
// TODO: Error reporting
size_t qb3_encode(encsp p, void* source, void* destination, qb3_mode mode) {
    if (p->quanta > 1)
        return encode_quanta(p, source, destination, mode);
    oBits s(reinterpret_cast<uint8_t*>(destination));
    int error_code = 0;
#define ENC(T) QB3::encode_best(reinterpret_cast<const T*>(source), s, *p)

    switch (mode) {
    case(qb3_mode::QB3_BEST):
        switch (p->type) {
        case qb3_dtype::QB3_U8:
        case qb3_dtype::QB3_I8:
            error_code = ENC(uint8_t); break;
        case qb3_dtype::QB3_U16:
        case qb3_dtype::QB3_I16:
            error_code = ENC(uint16_t); break;
        case qb3_dtype::QB3_U32:
        case qb3_dtype::QB3_I32:
            error_code = ENC(uint32_t); break;
        case qb3_dtype::QB3_U64:
        case qb3_dtype::QB3_I64:
            error_code = ENC(uint64_t); break;
        default:
            error_code = 3; // Invalid type
        } // data type
#undef ENC
        break;

    default: // encoding mode
#define ENC(T) QB3::encode_fast(reinterpret_cast<const T*>(source), s, *p)
        switch (p->type) {
        case qb3_dtype::QB3_U8:
        case qb3_dtype::QB3_I8:
            error_code = ENC(uint8_t); break;
        case qb3_dtype::QB3_U16:
        case qb3_dtype::QB3_I16:
            error_code = ENC(uint16_t); break;
        case qb3_dtype::QB3_U32:
        case qb3_dtype::QB3_I32:
            error_code = ENC(uint32_t); break;
        case qb3_dtype::QB3_U64:
        case qb3_dtype::QB3_I64:
            error_code = ENC(uint64_t); break;
        default:
            error_code = 3; // Invalid type
        } // data type
#undef ENC
    } // Encoding mode
    if (error_code)
        return 0;
    return (s.size_bits() + 7) / 8;
}
