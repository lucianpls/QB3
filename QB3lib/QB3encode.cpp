#include "QB3common.h"
#include "QB3encode.h"

struct encs {
    size_t xsize;
    size_t ysize;
    size_t nbands;
    size_t outsize;
    // band which will be subtracted, by band
    size_t cband[QB3_MAXBANDS];
    qb3_dtype type;
};

// constructor
encsp qb3_create_encoder(size_t width, size_t height, size_t bands, qb3_dtype dt) {
    if (width > 0x10000ul || height > 0x10000ul || bands == 0 || bands > QB3_MAXBANDS)
        return nullptr;
    auto p = new encs;
    p->xsize = width;
    p->ysize = height;
    p->nbands = bands;
    p->type = dt;
    p->outsize = 0;
    // Start with no inter-band differential
    for (size_t c = 0; c < bands; c++)
        p->cband[c] = c;
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

// bytes per value by qb3_dtype
static const int typesizes[] = { 1, 2, 4, 8 };

size_t qb3_max_encoded_size(const encsp p) {
    // Pad to 4 x 4
    size_t nvalues = 16 * ((p->xsize + 3) / 4) * ((p->ysize + 3) / 4) * p->nbands;
    // Maximum expansion is 17/16 bits per input value, for large number of values
    double bits_per_value = 17.0 / 16.0 + typesizes[static_cast<int>(p->type)] * 8;
    return 1024 + static_cast<size_t>(bits_per_value * nvalues / 8);
}

// The encode public API, returns 0 if an error is detected
// TODO: Error reporting
size_t qb3_encode(encsp p, void* source, void* destination, qb3_mode mode) {
    oBits s(reinterpret_cast<uint8_t*>(destination));
    int error_code = 0;
#define ENC(T) QB3::encode_best(reinterpret_cast<const T*>(source), s, p->xsize, p->ysize, p->nbands, p->cband)

    switch (mode) {

    case(qb3_mode::QB3_BEST):
        switch (p->type) {
        case qb3_dtype::QB3_I8:
            error_code = ENC(uint8_t); break;
        case qb3_dtype::QB3_I16:
            error_code = ENC(uint16_t); break;
        case qb3_dtype::QB3_I32:
            error_code = ENC(uint32_t); break;
        case qb3_dtype::QB3_I64:
            error_code = ENC(uint64_t); break;
        default:
            error_code = 3; // Invalid type
        } // data type
#undef ENC
        break;

    default: // encoding mode
#define ENC(T) QB3::encode_fast(reinterpret_cast<const T*>(source), s, p->xsize, p->ysize, p->nbands, p->cband)
        switch (p->type) {
        case qb3_dtype::QB3_I8:
            error_code = ENC(uint8_t); break;
        case qb3_dtype::QB3_I16:
            error_code = ENC(uint16_t); break;
        case qb3_dtype::QB3_I32:
            error_code = ENC(uint32_t); break;
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
