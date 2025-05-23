/*
Content: Public API for QB3 library

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

#if !defined(QB3_H)
// For size_t
#include <stddef.h>
// For uint64_t
#include <stdint.h>

// Defined when building the library
#if !defined(LIBQB3_EXPORT)
#define LIBQB3_EXPORT
#endif

// Keep this close to plain C so it can have a C API
#if defined(__cplusplus)
extern "C" {
#endif
// Max number of bands <= 256, changes size of the control structures
#define QB3_MAXBANDS 16

typedef struct encs * encsp; // encoder
typedef struct decs * decsp; // decoder

// Data types
enum qb3_dtype { QB3_U8 = 0, QB3_I8, QB3_U16, QB3_I16, QB3_U32, QB3_I32, QB3_U64, QB3_I64 };

// To check if the library has QB3M_FTL
#define QB3_HAS_FTL 1

// Encode mode
// Default is fastest, and faster decoding
// Base is barely better than FTL, 20% slower than FTL
// Best is best compression, 2x slower than base for encoding, 
//      slightly slower than base for decoding
enum qb3_mode {
    // Aliases, values might change
    QB3M_DEFAULT = 8, // FTL
    QB3M_BASE = 4,
    QB3M_BEST = 7,

    // Legacy z-curve
    QB3M_BASE_Z = 0, // Legacy base
    QB3M_CF = 1, //  + common factor
    QB3M_RLE = 2, // + RLE
    QB3M_CF_RLE = 3, // + CF + RLE

    // better, with Hilbert curve
    QB3M_BASE_H = 4, // Hilbert base
    QB3M_CF_H = 5, // Hilbert + CF
    QB3M_RLE_H = 6, // Hilbert + RLE
    QB3M_CF_RLE_H = 7, // Hilbert + CF + RLE

    // Faster and only slightly worse than base
    QB3M_FTL = 8, // Fastest, Hilbert base - step
    QB3M_END, // Marks the end of the settable modes

    QB3M_STORED = 255, // Raw bypass, can't be requested
    QB3M_INVALID = -1 // Invalid mode
}; // Best compression, one of the above

// Errors
enum qb3_error {
    QB3E_OK = 0,
    QB3E_EINV, // Invalid parameter
    QB3E_UNKN, // Unknown
    QB3E_ERR,  // unspecified error
    QB3E_LIBERR = 255 // internal QB3 error, should not happen
};

// In QB3encode.cpp

// Call before anything else
LIBQB3_EXPORT encsp qb3_create_encoder(size_t width, size_t height, size_t bands, qb3_dtype dt);
// Call when done with the encoder
LIBQB3_EXPORT void qb3_destroy_encoder(encsp p);

// Reset compression state, allowing encoder to be reused
// All settings are preserved
LIBQB3_EXPORT void qb3_reset_encoder(encsp p);

// Change the default core band mapping.
// The default assumes bands are independent, except for 3 or 4 bands
// when RGB(A) is assumed and R-G and B-G is used
// equivalent to cbands = { 1, 1, 1 }
// Returns false if band number differs from the one used to create p
// Only values < bands are acceptable in cband array
// The cband array might be modified if core bands are not valid or iterrative
LIBQB3_EXPORT bool qb3_set_encoder_coreband(encsp p, size_t bands, size_t *cband);

// Sets quantization parameters, returns true on success
// away = true -> round away from zero
LIBQB3_EXPORT bool qb3_set_encoder_quanta(encsp p, uint64_t q, bool away);

// Upper bound of encoded size, without taking the header into consideration
LIBQB3_EXPORT size_t qb3_max_encoded_size(const encsp p);

// Sets and returns the mode which will be used.
// If mode value is out of range, it returns the previous mode value of p
LIBQB3_EXPORT qb3_mode qb3_set_encoder_mode(encsp p, qb3_mode mode);

// Set line to line stride, in dtype units, defaults to xsize * nbands
LIBQB3_EXPORT void qb3_set_encoder_stride(encsp p, size_t stride);

// Encode the source into destination buffer, which should be at least qb3_max_encoded_size
// Source organization is expected to be y major, then x, then band (interleaved)
// Returns actual size, the encoder can be reused
LIBQB3_EXPORT size_t qb3_encode(encsp p, void *source, void *destination);

// Returns !0 if last encode call failed
LIBQB3_EXPORT int qb3_get_encoder_state(encsp p);


// In QB3decode.cpp

// Starts reading a formatted QB3 source. Reads the main header, which only contains the output size information
// returns nullptr if it fails, usually because the source is not in the correct format
// If successful, size containts 3 values, x size, y size and number of bands
LIBQB3_EXPORT decsp qb3_read_start(void* source, size_t source_size, size_t* image_size);

// Call after qb3_read_start, reads all headers until the raster data, returns false if it fails
LIBQB3_EXPORT bool qb3_read_info(decsp p);

// Call after qb3_read_info, reads all the data, returns bytes read
LIBQB3_EXPORT size_t qb3_read_data(decsp p, void* destination);

LIBQB3_EXPORT void qb3_destroy_decoder(decsp p);

LIBQB3_EXPORT size_t qb3_decoded_size(const decsp p);

LIBQB3_EXPORT qb3_dtype qb3_get_type(const decsp p);

// Set line to line stride, in dtype units, defaults to xsize * nbands
LIBQB3_EXPORT void qb3_set_decoder_stride(decsp p, size_t stride);

// Query settings, valid after qb3_read_info

// Encoding mode used, returns QB3M_INVALID if failed
LIBQB3_EXPORT qb3_mode qb3_get_mode(const decsp p);

// Returns the number of quantization bits used, returns 0 if failed
LIBQB3_EXPORT uint64_t qb3_get_quanta(const decsp p);

// Return the scanning curve used, returns 0 if failed
LIBQB3_EXPORT uint64_t qb3_get_order(const decsp p);

// Sets the cband array and returns true if successful
LIBQB3_EXPORT bool qb3_get_coreband(const decsp p, size_t *cband);

#if defined(__cplusplus)
}

#endif
#endif