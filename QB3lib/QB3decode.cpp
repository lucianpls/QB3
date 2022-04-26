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

// constructor
decsp qb3_create_decoder(size_t width, size_t height, size_t bands, qb3_dtype dt) {
    if (width > 0x10000ul || height > 0x10000ul || bands == 0 || bands > QB3_MAXBANDS)
        return nullptr;
    auto p = new decs;
    p->xsize = width;
    p->ysize = height;
    p->nbands = bands;
    p->type = dt;
    p->quanta = 0;
    p->raw = false;
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
    T q = static_cast<T>(p->quanta);
    static const T maxt = std::numeric_limits<T>::max();
    static const T mint = std::numeric_limits<T>::min();
    T mai = maxt / q; // Top valid value
    T mii = mint / q; // Bottom valid value
    for (size_t i = 0; i < sz; i++) {
        auto data = d[i];
        d[i] = (data <= mai) * (data * q) + (!(data <= mai)) * maxt;
        if (std::is_signed<T>() && q > 2 && (data < mii))
            d[i] = mint;
    }
}

// Check a 4 byte signature
bool check_sig(uint64_t val, const uint8_t *sig) {
    for (int i = 0; i < 4; i++) {
        if (static_cast<uint8_t>(val & 0xff) != sig[i])
            return false;
        val >>= 8;
    }
    return true;
}

bool read_headers(decsp p, iBits& s) {
    p->error = QB3E_OK;
    auto val = s.peek();
    constexpr uint8_t qb3sig[] = {'Q', 'B', '3', 128};
    if (!check_sig(val, qb3sig))
        return false;
    val >>= 32;
    p->xsize = val & 0xffffull;
    p->xsize++;
    val >>= 16;
    p->ysize = val & 0xffffull;
    p->ysize++;
    s.advance(64);
    val = s.peek();
    p->nbands = val & 0xff;
    if (p->nbands >= QB3_MAXBANDS) {
        p->nbands = 0;
        p->error = QB3E_EINV;
        return false;
    }
    val >>= 8;
    p->mode = static_cast<qb3_mode>(val & 0xff);
    if (p->mode > QB3M_BEST) {
        p->error = QB3E_EINV;
        p->mode = QB3M_DEFAULT;
        return false;
    }
    s.advance(16); // Two bytes
    // Need a few headers
    do {

    } while (QB3E_OK != p->error && !s.empty());
    return true;
}

// The encode public API, returns 0 if an error is detected
// TODO: Error reporting
size_t qb3_decode(decsp p, void* source, size_t src_sz, void* destination) {

    int error_code = 0;
    auto src = reinterpret_cast<uint8_t *>(source);
    if (!p->raw) {
        iBits s(src, src_sz);
        if (!read_headers(p, s))
            return 0;
    }

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
