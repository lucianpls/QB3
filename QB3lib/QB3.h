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

Content: Forward definition headers, to be used when linking with QB3 library

Contributors:  Lucian Plesea
*/

#pragma once
// For size_t
#include <stddef.h>

#if defined(_WIN32)
#if defined(libQB3_EXPORTS)
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __declspec(dllimport)
#endif
#else
// Not windows
#define DLLEXPORT 
#endif

// Keep this close to plain C so it can have a C API
#define QB3_MAXBANDS 16

#if defined(__cplusplus)
extern "C" {
#endif
typedef struct encs * encsp; // encoder
typedef struct decs * decsp; // decoder

// Types
constexpr enum qb3_dtype { QB3_U8 = 0, QB3_I8, QB3_U16, QB3_I16, QB3_U32, QB3_I32, QB3_U64, QB3_I64 };
// Encode mode
constexpr enum qb3_mode { QB3M_DEFAULT = 0, QB3M_BASE = 0, QB3M_BEST };

// Errors
constexpr enum qb3_error {
    QB3E_OK = 0,
    QB3E_EINV, // Invalid parameter
    QB3E_UNKN, // Unknown 
    QB3E_ERR   // Last, unspecified error
};

// In QB3encode.cpp

// Call before anything else
DLLEXPORT encsp qb3_create_encoder(size_t width, size_t height, size_t bands, int dt);
// Call when done with the encoder
DLLEXPORT void qb3_destroy_encoder(encsp p);

// Change the default core band mapping.
// The default assumes bands are independent, except for 3 or 4 bands
// when RGB(A) is assumed and R-G and B-G is used
// equivalent to cbands = { 1, 1, 1 }
// Returns false if band number differs from the one used to create p
// Only values < bands are acceptable in cband array
// The cband array might be modified if core bands are not valid or iterrative
DLLEXPORT bool qb3_set_encoder_coreband(encsp p, size_t bands, size_t *cband);

// Sets quantization parameters, returns true on success
// away = true -> round away from zero
DLLEXPORT bool qb3_set_encoder_quanta(encsp p, size_t q, bool away);

// Upper bound of encoded size, without taking the header into consideration
DLLEXPORT size_t qb3_max_encoded_size(const encsp p);

// Sets and returns the mode which will be used.
// If mode value is out of range, it returns the previous mode value of p
DLLEXPORT qb3_mode qb3_set_encoder_mode(encsp p, qb3_mode mode);

// Generate raw qb3 stream, no headers
DLLEXPORT void qb3_set_encoder_raw(encsp p);

// Encode the source into destination buffer, which should be at least qb3_max_encoded_size
// Source organization is expected to be y major, then x, then band (interleaved)
// Returns actual size, the encoder can be reused
DLLEXPORT size_t qb3_encode(encsp p, void *source, void *destination);

// Returns !0 if last encode call failed
DLLEXPORT int qb3_get_encoder_state(encsp p);


// In QB3decode.cpp

// To be used by raw and formatted QB3 streams
DLLEXPORT void qb3_destroy_decoder(decsp p);

// Raw functions
DLLEXPORT decsp qb3_create_raw_decoder(size_t width, size_t height, size_t bands, qb3_dtype dt);

DLLEXPORT size_t qb3_decode(decsp p, void* source, size_t src_sz, void* destination);

DLLEXPORT size_t qb3_decoded_size(const decsp p);

DLLEXPORT qb3_dtype qb3_get_type(const decsp p);

// Formatted stream decode functions

// Starts reading a formatted QB3 source. Reads the main header, which only contains the output size information
// returns nullptr if it fails, usually because the source is not in the correct format
// If successful, size containts 3 values, x size, y size and number of bands
DLLEXPORT decsp qb3_read_start(void* source, size_t source_size, size_t* image_size);

// Call after qb3_read_start, reads all headers until the raster data, returns false if it fails
DLLEXPORT bool qb3_read_info(decsp p);

// Call after qb3_read_info, reads all the data, returns bytes read
DLLEXPORT size_t qb3_read_data(decsp p, void* destination);

#if defined(__cplusplus)
}

#endif
