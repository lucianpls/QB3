/*
Copyright 2021 Esri
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
#include <cinttypes>

#if defined(_WIN32)
#if defined(QB3_EXPORTS)
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __declspec(dllimport)
#endif
#else
// Not windows
#endif

// Keep this close to plain C so it can have a C API
#define QB3_MAXBANDS 10

typedef struct encs * encsp; // encoder
typedef struct decs * decsp; // decoder

// Types, can be used with either signed or unsigned
enum class qb3_dtype { QB3_I8 = 0, QB3_I16, QB3_I32, QB3_I64 };
// Encode mode
enum class qb3_mode { QB3_DEFAULT = 0, QB3_BASE = 0, QB3_BEST};

// In QB3encode.cpp
// Call before anything else
DLLEXPORT encsp qb3_create_encoder(size_t width, size_t height, size_t bands = 3, qb3_dtype dt = qb3_dtype::QB3_I8);
// Call when done with the encoder
DLLEXPORT void qb3_destroy_encoder(encsp p);

// Upper bound of encoded size, without taking the header into consideration
DLLEXPORT size_t qb3_max_encoded_size(const encsp p);

// One call QB3 lossless encode the source into destination, (at least max_encoded_size())
// Source organization is expected to be y major, then x, then band (interleaved)
// Returns actual size
// encoder structure can be reused
DLLEXPORT size_t qb3_encode(encsp p, void *source, void *destination, qb3_mode mode = qb3_mode::QB3_DEFAULT);

// In QB3decode.cpp

DLLEXPORT decsp qb3_create_decoder(size_t width, size_t height, size_t bands = 3, qb3_dtype dt = qb3_dtype::QB3_I8);

DLLEXPORT void qb3_destroy_decoder(decsp p);

DLLEXPORT size_t qb3_decoded_size(const decsp p);

DLLEXPORT size_t qb3_decode(decsp p, void* source, size_t src_sz, void* destination);