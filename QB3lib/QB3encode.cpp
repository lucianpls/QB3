/*
Content: C API QB3 encoding

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
#include "QB3encode.h"
#include <limits>
#include <vector>
// For memcpy
#include <cstring>

// constructor
encsp qb3_create_encoder(size_t width, size_t height, size_t bands, qb3_dtype dt) {
    if (width < 4 || width > 0x10000ul 
        || height < 4 || height > 0x10000ul 
        || bands == 0 || bands > QB3_MAXBANDS 
        || dt > int(QB3_I64))
        return nullptr;
    auto p = new encs;
    p->xsize = width;
    p->ysize = height;
    p->nbands = bands;
    p->order = 0; // will get changed to HILBERT
    p->type = static_cast<qb3_dtype>(dt);
    p->quanta = 1; // No quantization
    p->away = false; // Round to zero
    p->mode = QB3M_DEFAULT; // Fast
    // Start with no inter-band differential
    for (size_t c = 0; c < bands; c++)
        p->cband[c] = c;
    // Assume RGB(A) input and use R-G,G,B-G
    if (bands == 3 || bands == 4)
        p->cband[0] = p->cband[2] = 1;
    qb3_reset_encoder(p);
    return p;
}

void qb3_reset_encoder(encsp p) {
    for (size_t c = 0; c < p->nbands; c++) {
        p->band[c].runbits = 0;
        p->band[c].prev = 0;
        p->band[c].cf = 0;
    }
    p->error = 0;
}

void qb3_destroy_encoder(encsp p) {
    delete p;
}

bool qb3_set_encoder_coreband(encsp p, size_t bands, size_t *cband) {
    if (bands != p->nbands)
        return false; // Incorrect band number
    // Set it, make sure it's not out of spec
    for (size_t i = 0; i < bands; i++)
        p->cband[i] = static_cast<uint8_t>((cband[i] < bands) ? cband[i] : i);
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
    if (q < 1)
        return false;
    p->quanta = q;
    p->away = away;
    if (q == 1) // No quantization
        return true;
    // Check the quanta value agains the max positive by type
    bool error = false;
    switch (p->type) {
#define TOO_LARGE(Q, T) (Q > uint64_t(std::numeric_limits<T>::max()))
    case QB3_I8:
        error |= TOO_LARGE(p->quanta, int8_t);
    case QB3_U8:
        error |= TOO_LARGE(p->quanta, uint8_t);
    case QB3_I16:
        error |= TOO_LARGE(p->quanta, int16_t);
    case QB3_U16:
        error |= TOO_LARGE(p->quanta, uint16_t);
    case QB3_I32:
        error |= TOO_LARGE(p->quanta, int32_t);
    case QB3_U32:
        error |= TOO_LARGE(p->quanta, uint32_t);
    case QB3_I64:
        error |= TOO_LARGE(p->quanta, int64_t);
    default: // Makes clang -Wswitch happy
        break;
    } // data type
#undef TOO_LARGE
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
    if (mode >= qb3_mode(0) && mode < qb3_mode::QB3M_END)
        p->mode = mode;
    // Default curve is HILBERT, change it if needed
    switch (p->mode) {
    case QB3M_BASE_Z:
    case QB3M_CF:
    case QB3M_CF_RLE:
    case QB3M_RLE:
        p->order = ZCURVE;
    default: // Makes clang -Wswitch happy
        break;
    }
    return p->mode;
}

// Round to Zero Division, no overflow
template<typename T> static
T rounddiv(T n, T d) {
    T m = n % d;
    T h = d / 2;
    return n / d + (!(n < 0) & (m > h)) - ((n < 0) & ((m + h) < 0));
}

// Round from Zero Division, no overflow
template<typename T> static
T rounddiv_away(T n, T d) {
    T m = n % d;
    T h = d / 2 + d % 2;
    return n / d + (!(n < 0) & (m >= h)) - ((n < 0) & ((m + h) <= 0));
}

// Quantize source, in place. T may be signed, q is positive
template<typename T> static
bool quantize(T* source, encs& p) {
    size_t nV = p.xsize * p.ysize * p.nbands; // Number of values
    T q = static_cast<T>(p.quanta);
    // Optimized versions have to preserve the sign of the input
    if (2 == q) {
        if (p.away)
            for (size_t i = 0; i < nV; i++)
                source[i] = source[i] / T(2) + source[i] % T(2);
        else
            for (size_t i = 0; i < nV; i++)
                source[i] /= T(2);
    }
    else if (3 == q) {
        for (size_t i = 0; i < nV; i++)
            source[i] = source[i] / T(3) + (source[i] % T(3)) / T(2);
    }
    else if (4 == q) {
        if (p.away)
            for (size_t i = 0; i < nV; i++)
                source[i] = source[i] / T(4) + (source[i] % T(4)) / T(2);
        else
            for (size_t i = 0; i < nV; i++)
                source[i] = source[i] / T(4) + (source[i] % T(4)) / T(3);
    }
    // General case
    else if (p.away) {
        for (size_t i = 0; i < nV; i++)
            source[i] = rounddiv_away(source[i], q);
    }
    else {
        for (size_t i = 0; i < nV; i++)
            source[i] = rounddiv(source[i], q);
    }
    return false;
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

//
// TODO: Expose the known headers
// 
// They are somewhat similar to the PNG chunk names
// Currently they are: "CB", "QV", "SC", "DT"
// If the first letter is lower case, it can be ignored
//


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
void static write_quanta_header(encsp p, oBits& s) {
    if (p->quanta < 2) // Is it needed
        return;
    push_sig("QV", s);
    size_t qbytes = 1 + topbit(p->quanta) / 8;
    s.push(qbytes, 16); // Payload bytes, 0 < v < 5
    s.push(p->quanta, qbytes * 8);
}

// Write the encoding curve, if it's not the legacy Morton
void static write_scanning_curve(encsp p, oBits& s) {
    if (p->order == ZCURVE || p->mode == QB3M_STORED)
        return;
    push_sig("SC", s);
    s.push(8u, 16); // Always 64 bits
    s.push(p->order ? p->order : HILBERT, 64);
}

// Data header has no known size
void static write_data_header(encsp, oBits& s) {
    push_sig("DT", s);
}

// Writes the headers, in the right order
// Implicitly it defines the order of the headers,
// which should end with the data header
void static write_headers(encsp p, oBits& s) {
    write_qb3_header(p, s);
    write_cband_header(p, s);
    write_quanta_header(p, s);
    write_scanning_curve(p, s);
    write_data_header(p, s);
}

// Returns the number of bytes of value c
static uint8_t run_count(const uint8_t* s, uint8_t c, uint8_t len = 0xff) {
    for (uint8_t i = 0; i < len; i++, s++)
        if (c != *s)
            return i;
    return len;
}

// Special purpose RLE encoder, only packs long sequnces of 0
// Uses 0xff 0xff as a marker
// It generates the following special patterns
// FF FF FF ->> FF FF
// FF FF N ->> 0 repeated N + 4 times, between 4 and 258 
//

static size_t RLE0FFFF(const uint8_t* src, size_t len, uint8_t* dst) {
    uint8_t* d(dst);
    while (len--) {
        const uint8_t c = *src++; // non-special or last two bytes are always copied
        if (((c + 1) & 0xfe) || (2 > len)) {
            *d++ = c;
        }
        else { // Special char, at least one more is available in src
            uint8_t cc = *src; // not consumed here
            if (c != cc) { // non-repeating special
                *d++ = c; // Copy the first char, not special enough
                continue;
            }
            // At least two special chars in a row
            src++; // Consume the second byte, it will be handled
            len--;

            if (c) { // Two FFs in a row, encoded as FF FF FF
                *d++ = c;
                *d++ = c;
                *d++ = c;
                continue;
            }
            // Two zeros, if we don't have four it's not a run
            if (2 > len || 0 != src[0] || 0 != src[1]) { // Not four zeros, emit two
                *d++ = 0;
                *d++ = 0;
                continue;
            }

            // If the last emitted byte was FF this can't be encoded as a run
            // emit one zero and put back the second
            if (d != dst && 0xff == d[-1]) {
                *d++ = 0;
                len++;
                src--;
                continue;
            }

            // at least four zeros on input, use the next two
            src += 2;
            len -= 2;
            auto run = static_cast<uint8_t>(len < 0xff ? len : 0xfe); // Can't use 0xff
            run = run_count(src, 0, run); // In addition to the four
            // run, emit FF FF run (run is at least 4)
            *d++ = 0xff;
            *d++ = 0xff;
            *d++ = run; // Signifying the 4 to 258 range
            src += run;
            len -= run;
        }
    }
    return d - dst;
}

// Returns the size of the packed data, without writing anything
static size_t RLE0FFFFSize(const uint8_t* src, size_t len) {
    uint8_t last(0); // Last byte emitted, to avoid encoding runs of FFs
    size_t count(0);
    while (len--) {
        const uint8_t c = *src++; // non-special or last two bytes are always copied
        if (((c + 1) & 0xfe) || (2 > len)) {
            count++;
            last = c;
        }
        else { // Special char, at least one more is available in src
            uint8_t cc = *src; // not consumed here
            if (c != cc) { // non-repeating special
                last = c;
                count++;
                continue;
            }
            // At least two special chars in a row
            src++; // Consume the second byte, it will be handled
            len--;

            if (c) { // Two FFs in a row, encoded as FF FF FF
                last = 0xff;
                count += 3;
                continue;
            }
            // Two zeros, if we don't have four it's not a run
            if (2 > len || 0 != src[0] || 0 != src[1]) { // Not four zeros, emit two
                last = 0;
                count += 2;
                continue;
            }

            // If the last emitted byte was FF this can't be encoded as a run
            // emit one zero and put back the second
            if (0xff == last) {
                last = 0;
                count++;
                len++;
                src--;
                continue;
            }

            // at least four zeros on input, use the next two
            src += 2;
            len -= 2;
            uint8_t run = static_cast<uint8_t>(len < 0xff ? len : 0xfe); // Can't use 0xff
            run = run_count(src, 0, run); // In addition to the four
            // run, emit FF FF run (run is at least 4)
            last = run;
            count += 3;
            src += run;
            len -= run;
        }
    }
    return count;
}

static size_t raw_size(encsp const &p) {
    return p->xsize * p->ysize * p->nbands * typesizes[p->type];
}

int qb3_get_encoder_state(encsp p) { return p->error; }

static bool is_fast(qb3_mode mode) {
    return (QB3M_BASE_H == mode) || (QB3M_BASE_Z == mode) || (QB3M_FTL == mode);
}

// ONLY QB3M_BASE and QB3M_CF are supported here
template<typename T> static int enc(const T *source, oBits &s, encsp p)
{
    int error(0);
    if (p->quanta < 2) {
        if (is_fast(p->mode))
            return QB3::encode_fast(source, s, *p);
        else
            return QB3::encode_best(source, s, *p);
    }

    // Quantized encoding
    // Use a subencoder to encode one B lines strip at a time,
    // while keeping the running state from one strip to the next
    // This avoids doubling memory for the input data
    encs subimg(*p);
    subimg.ysize = B;
    auto ysz(p->ysize);
    // In bytes
    auto linesize = p->xsize * p->nbands * typesizes[p->type];
    // Temporary data buffer for a single strip
    std::vector<uint8_t> buffer(linesize * subimg.ysize);
    auto src = reinterpret_cast<const uint8_t*>(source);

#define QENC(T)\
    quantize(reinterpret_cast<T *>(buffer.data()), subimg);\
    if (is_fast(subimg.mode))\
        error = QB3::encode_fast(\
            reinterpret_cast<std::make_unsigned<T>::type *>(buffer.data()), s, subimg);\
    else\
        error = QB3::encode_best(\
            reinterpret_cast<std::make_unsigned<T>::type *>(buffer.data()), s, subimg);

    for (size_t y = 0; y < ysz; y += subimg.ysize) {
        // Shift the last strip up to handle the edge
        if (y + subimg.ysize > ysz)
            src -= linesize * (y + subimg.ysize - ysz);
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
        src += linesize * subimg.ysize;
    }

#undef QENC
    return error;
}

// The encode public API, returns 0 if an error is detected
size_t qb3_encode(encsp p, void* source, void* destination) {
    auto const mode = p->mode; // save the user chosen mode
    // Turn off the RLE for now
    bool rle = (QB3M_RLE == mode || QB3M_CF_RLE == mode || QB3M_CF_RLE_H == mode || QB3M_RLE_H == mode);
    if (rle) {
        switch (mode) {
        case QB3M_RLE:
            p->mode = QB3M_BASE_Z;
            break;
        case QB3M_CF_RLE:
            p->mode = QB3M_CF;
            break;
        case QB3M_RLE_H:
            p->mode = QB3M_BASE_H;
            break;
        case QB3M_CF_RLE_H:
            p->mode = QB3M_CF_H;
            break;
        default: // Library internal error
            p->error = 255;
            return 0;
        }
    }

    uint8_t* const d = reinterpret_cast<uint8_t*>(destination);
    oBits s(d);
    // size of headers
    size_t data_position(0);
    write_headers(p, s);
    data_position = (s.position() + 7) / 8; // It is byte aligned already
    if (p->error) return 0;

#define ENC(T) enc(reinterpret_cast<const T*>(source), s, p)
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

    auto len = (s.position() + 7) / 8; // current output position in bytes
    if (rle) {
        p->mode = mode; // restore the user selected mode that includes RLE
        if (p->error) // Bail out if there was an error
            return 0;

        // Skip RLE if the compression is poor, this is a vague limit
        // RLE is only efficient if data is const, which generates a 8:1 compression ratio
        if (len <= qb3_max_encoded_size(p) / 2) {
            auto data_size = len - data_position; // Exclude the headers, they will be rewritten
            auto available = qb3_max_encoded_size(p) - len;
            auto rle_size = RLE0FFFFSize(d + data_position, data_size);
            if (rle_size <= available && rle_size < data_size) { // Only if it fits and is small enough
                // Encode it at the end of the data
                auto new_size = RLE0FFFF(d + data_position, data_size, d + len);
                // Check that it worked
                if (new_size != rle_size) { // Paranoid check
                    p->error = QB3E_EINV; // Something went wrong, bail out
                    return 0;
                }

                // new stream, same buffer
                oBits srle(d);
                write_headers(p, srle);
                if (p->error)
                    return 0;
                // Copy the RLE encoded data at the current position, they are not overlapping
                memcpy(d + srle.tobyte(), d + len, rle_size);
                // Return the new size
                return srle.tobyte() + rle_size;
            }
        }
    }

    // Maybe stored mode is better
    if (!p->error && raw_size(p) <= len) {
        // new stream, same buffer
        oBits sraw(d);
        p->mode = QB3M_STORED; // Force raw mode
        write_headers(p, sraw);
        if (p->error)
            return 0;
        // Copy the raw data at the current position, they are not overlapping
        memcpy(d + sraw.tobyte(), source, raw_size(p));
        p->mode = mode; // restore the user selected mode, in case of reuse
        // Return the new size
        return sraw.tobyte() + raw_size(p);
    }
    return (p->error) ? 0 : s.tobyte();
}
