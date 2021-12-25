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
class oBits;
class iBits;

namespace QB3 {
    // Encode image into an output bitstream, using only basic QB3
    template <typename T>
    bool encode_fast(const T* image, oBits& s,
        size_t xsize, size_t ysize, size_t bands, int mb = 1);

    // Encode image into an output bitstream, using basic QB3 and common factor
    // This method has better compression but is slower than encode_fast
    template <typename T>
    bool encode_best(const T* image, oBits& s,
        size_t xsize, size_t ysize, size_t bands, int mb = 1);

    //
    template<typename T>
    bool decode(uint8_t *src, size_t len, T* image, 
        size_t xsize, size_t ysize, size_t bands, int mb = 1);
}