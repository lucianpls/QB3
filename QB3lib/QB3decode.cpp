#include "QB3common.h"
#include "QB3decode.h"

struct decs {
    size_t xsize;
    size_t ysize;
    size_t nbands;
    size_t outsize;
    // band which will be subtracted, by band
    size_t cband[QB3_MAXBANDS];
    qb3_dtype type;
};

// constructor
decsp qb3_create_decoder(size_t width, size_t height, size_t bands, qb3_dtype dt) {
    if (width > 0x10000ul || height > 0x10000ul || bands == 0 || bands > QB3_MAXBANDS)
        return nullptr;
    auto p = new decs;
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

// bytes per value by qb3_dtype
static const int typesizes[] = { 1, 2, 4, 8 };

size_t qb3_decoded_size(const decsp p) {
    return p->xsize * p->ysize * p->nbands * typesizes[static_cast<int>(p->type)];
}

// The encode public API, returns 0 if an error is detected
// TODO: Error reporting
size_t qb3_decode(decsp p, void* source, size_t src_sz, void* destination) {

    int error_code = 0;
    auto src = reinterpret_cast<uint8_t *>(source);

#define DEC(T) QB3::decode(src, src_sz, reinterpret_cast<T*>(destination), p->xsize, p->ysize, p->nbands, p->cband)

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
#undef ENC

    if (error_code)
        return 0;
    return qb3_decoded_size(p);
}
