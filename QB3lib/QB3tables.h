/*
Copyright 2020-2021 Esri
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

#pragma once
#include <cinttypes>

// TODO: Split the tables into decoding and encoding ones, so the decoder can be built by itself
// The SIGNAL table is shared

namespace QB3 {
    extern const uint8_t xlut[16], ylut[16];
// Encoding and decoding tables for low bit sizes
// The 1-7 (byte) tables are always defined
// tables for 8, 9 and 10 bits are optional

#if defined(QB3_SHORT_TABLES)
    extern const uint16_t* CRG[8];
    extern const uint16_t* DRG[8];
#else
    extern const uint16_t* CRG[11];
    extern const uint16_t* DRG[11];
#endif

    // Double decoding tables, rungs 1 and 2
    extern const uint8_t DDRG1[64];
    extern const uint16_t DDRG2[256];

    // Dispatch tables for rungs, by bits/unit
    extern const uint16_t SIGNAL[7]; // switch to same rung, used as a signal
    extern const uint16_t* CSW[7];
    extern const uint16_t* DSW[7];

}