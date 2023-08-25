/*
Copyright 2020-2023 Esri
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Content: QB3 encoding

Contributors:  Lucian Plesea
*/

#pragma once
#include "QB3common.h"
#include <algorithm>

namespace QB3 {
// Encoding tables for rungs up to 8, for speedup. Rung 0 and 1 are special
// Storage is under 1K
// Tables could be generated for more rungs, but the speedup is not worth it
// See tables.py for how they are generated
static const uint16_t crg0[] = { 0x1000, 0x1001 };
static const uint16_t crg1[] = { 0x1000, 0x2001, 0x3003, 0x3007 };
static const uint16_t crg2[] = { 0x2000, 0x2002, 0x3001, 0x3005, 0x4003, 0x4007, 0x400b, 0x400f };
static const uint16_t crg3[] = { 0x3000, 0x3002, 0x3004, 0x3006, 0x4001, 0x4005, 0x4009, 0x400d, 0x5003, 0x5007, 0x500b, 0x500f,
0x5013, 0x5017, 0x501b, 0x501f };
static const uint16_t crg4[] = { 0x4000, 0x4002, 0x4004, 0x4006, 0x4008, 0x400a, 0x400c, 0x400e, 0x5001, 0x5005, 0x5009, 0x500d,
0x5011, 0x5015, 0x5019, 0x501d, 0x6003, 0x6007, 0x600b, 0x600f, 0x6013, 0x6017, 0x601b, 0x601f, 0x6023, 0x6027, 0x602b, 0x602f,
0x6033, 0x6037, 0x603b, 0x603f };
static const uint16_t crg5[] = { 0x5000, 0x5002, 0x5004, 0x5006, 0x5008, 0x500a, 0x500c, 0x500e, 0x5010, 0x5012, 0x5014, 0x5016,
0x5018, 0x501a, 0x501c, 0x501e, 0x6001, 0x6005, 0x6009, 0x600d, 0x6011, 0x6015, 0x6019, 0x601d, 0x6021, 0x6025, 0x6029, 0x602d,
0x6031, 0x6035, 0x6039, 0x603d, 0x7003, 0x7007, 0x700b, 0x700f, 0x7013, 0x7017, 0x701b, 0x701f, 0x7023, 0x7027, 0x702b, 0x702f,
0x7033, 0x7037, 0x703b, 0x703f, 0x7043, 0x7047, 0x704b, 0x704f, 0x7053, 0x7057, 0x705b, 0x705f, 0x7063, 0x7067, 0x706b, 0x706f,
0x7073, 0x7077, 0x707b, 0x707f };
static const uint16_t crg6[] = { 0x6000, 0x6002, 0x6004, 0x6006, 0x6008, 0x600a, 0x600c, 0x600e, 0x6010, 0x6012, 0x6014, 0x6016,
0x6018, 0x601a, 0x601c, 0x601e, 0x6020, 0x6022, 0x6024, 0x6026, 0x6028, 0x602a, 0x602c, 0x602e, 0x6030, 0x6032, 0x6034, 0x6036,
0x6038, 0x603a, 0x603c, 0x603e, 0x7001, 0x7005, 0x7009, 0x700d, 0x7011, 0x7015, 0x7019, 0x701d, 0x7021, 0x7025, 0x7029, 0x702d,
0x7031, 0x7035, 0x7039, 0x703d, 0x7041, 0x7045, 0x7049, 0x704d, 0x7051, 0x7055, 0x7059, 0x705d, 0x7061, 0x7065, 0x7069, 0x706d,
0x7071, 0x7075, 0x7079, 0x707d, 0x8003, 0x8007, 0x800b, 0x800f, 0x8013, 0x8017, 0x801b, 0x801f, 0x8023, 0x8027, 0x802b, 0x802f,
0x8033, 0x8037, 0x803b, 0x803f, 0x8043, 0x8047, 0x804b, 0x804f, 0x8053, 0x8057, 0x805b, 0x805f, 0x8063, 0x8067, 0x806b, 0x806f,
0x8073, 0x8077, 0x807b, 0x807f, 0x8083, 0x8087, 0x808b, 0x808f, 0x8093, 0x8097, 0x809b, 0x809f, 0x80a3, 0x80a7, 0x80ab, 0x80af,
0x80b3, 0x80b7, 0x80bb, 0x80bf, 0x80c3, 0x80c7, 0x80cb, 0x80cf, 0x80d3, 0x80d7, 0x80db, 0x80df, 0x80e3, 0x80e7, 0x80eb, 0x80ef,
0x80f3, 0x80f7, 0x80fb, 0x80ff };
static const uint16_t crg7[] = { 0x7000, 0x7002, 0x7004, 0x7006, 0x7008, 0x700a, 0x700c, 0x700e, 0x7010, 0x7012, 0x7014, 0x7016,
0x7018, 0x701a, 0x701c, 0x701e, 0x7020, 0x7022, 0x7024, 0x7026, 0x7028, 0x702a, 0x702c, 0x702e, 0x7030, 0x7032, 0x7034, 0x7036,
0x7038, 0x703a, 0x703c, 0x703e, 0x7040, 0x7042, 0x7044, 0x7046, 0x7048, 0x704a, 0x704c, 0x704e, 0x7050, 0x7052, 0x7054, 0x7056,
0x7058, 0x705a, 0x705c, 0x705e, 0x7060, 0x7062, 0x7064, 0x7066, 0x7068, 0x706a, 0x706c, 0x706e, 0x7070, 0x7072, 0x7074, 0x7076,
0x7078, 0x707a, 0x707c, 0x707e, 0x8001, 0x8005, 0x8009, 0x800d, 0x8011, 0x8015, 0x8019, 0x801d, 0x8021, 0x8025, 0x8029, 0x802d,
0x8031, 0x8035, 0x8039, 0x803d, 0x8041, 0x8045, 0x8049, 0x804d, 0x8051, 0x8055, 0x8059, 0x805d, 0x8061, 0x8065, 0x8069, 0x806d,
0x8071, 0x8075, 0x8079, 0x807d, 0x8081, 0x8085, 0x8089, 0x808d, 0x8091, 0x8095, 0x8099, 0x809d, 0x80a1, 0x80a5, 0x80a9, 0x80ad,
0x80b1, 0x80b5, 0x80b9, 0x80bd, 0x80c1, 0x80c5, 0x80c9, 0x80cd, 0x80d1, 0x80d5, 0x80d9, 0x80dd, 0x80e1, 0x80e5, 0x80e9, 0x80ed,
0x80f1, 0x80f5, 0x80f9, 0x80fd, 0x9003, 0x9007, 0x900b, 0x900f, 0x9013, 0x9017, 0x901b, 0x901f, 0x9023, 0x9027, 0x902b, 0x902f,
0x9033, 0x9037, 0x903b, 0x903f, 0x9043, 0x9047, 0x904b, 0x904f, 0x9053, 0x9057, 0x905b, 0x905f, 0x9063, 0x9067, 0x906b, 0x906f,
0x9073, 0x9077, 0x907b, 0x907f, 0x9083, 0x9087, 0x908b, 0x908f, 0x9093, 0x9097, 0x909b, 0x909f, 0x90a3, 0x90a7, 0x90ab, 0x90af,
0x90b3, 0x90b7, 0x90bb, 0x90bf, 0x90c3, 0x90c7, 0x90cb, 0x90cf, 0x90d3, 0x90d7, 0x90db, 0x90df, 0x90e3, 0x90e7, 0x90eb, 0x90ef,
0x90f3, 0x90f7, 0x90fb, 0x90ff, 0x9103, 0x9107, 0x910b, 0x910f, 0x9113, 0x9117, 0x911b, 0x911f, 0x9123, 0x9127, 0x912b, 0x912f,
0x9133, 0x9137, 0x913b, 0x913f, 0x9143, 0x9147, 0x914b, 0x914f, 0x9153, 0x9157, 0x915b, 0x915f, 0x9163, 0x9167, 0x916b, 0x916f,
0x9173, 0x9177, 0x917b, 0x917f, 0x9183, 0x9187, 0x918b, 0x918f, 0x9193, 0x9197, 0x919b, 0x919f, 0x91a3, 0x91a7, 0x91ab, 0x91af,
0x91b3, 0x91b7, 0x91bb, 0x91bf, 0x91c3, 0x91c7, 0x91cb, 0x91cf, 0x91d3, 0x91d7, 0x91db, 0x91df, 0x91e3, 0x91e7, 0x91eb, 0x91ef,
0x91f3, 0x91f7, 0x91fb, 0x91ff };

static const uint16_t* CRG[] = { crg0, crg1, crg2, crg3, crg4, crg5, crg6, crg7 };

// Code switch encoding tables, about 256 bytes, stored the same way
// See tables.py for how they are generated
// They are defined for 3 - 6 bits for unit length. 0x1000 means no change
static const uint16_t csw3[] = { 0x1000, 0x3001, 0x4003, 0x5007, 0x501f, 0x500f, 0x400b, 0x3005 };
static const uint16_t csw4[] = { 0x1000, 0x4001, 0x4009, 0x5003, 0x5013, 0x6007, 0x6017, 0x6027, 0x603f, 0x602f, 0x601f, 0x600f,
0x501b, 0x500b, 0x400d, 0x4005 };
static const uint16_t csw5[] = { 0x1000, 0x5001, 0x5009, 0x5011, 0x5019, 0x6003, 0x6013, 0x6023, 0x6033, 0x7007, 0x7017, 0x7027,
0x7037, 0x7047, 0x7057, 0x7067, 0x707f, 0x706f, 0x705f, 0x704f, 0x703f, 0x702f, 0x701f, 0x700f, 0x603b, 0x602b, 0x601b, 0x600b,
0x501d, 0x5015, 0x500d, 0x5005 };
static const uint16_t csw6[] = { 0x1000, 0x6001, 0x6009, 0x6011, 0x6019, 0x6021, 0x6029, 0x6031, 0x6039, 0x7003, 0x7013, 0x7023,
0x7033, 0x7043, 0x7053, 0x7063, 0x7073, 0x8007, 0x8017, 0x8027, 0x8037, 0x8047, 0x8057, 0x8067, 0x8077, 0x8087, 0x8097, 0x80a7,
0x80b7, 0x80c7, 0x80d7, 0x80e7, 0x80ff, 0x80ef, 0x80df, 0x80cf, 0x80bf, 0x80af, 0x809f, 0x808f, 0x807f, 0x806f, 0x805f, 0x804f,
0x803f, 0x802f, 0x801f, 0x800f, 0x707b, 0x706b, 0x705b, 0x704b, 0x703b, 0x702b, 0x701b, 0x700b, 0x603d, 0x6035, 0x602d, 0x6025,
0x601d, 0x6015, 0x600d, 0x6005 };

// integer divide val(in magsign) by cf(normal)
template<typename T> static T magsdiv(T val, T cf) {return ((magsabs(val) / cf) << 1) - (val & 1);}

// greatest common factor (absolute) of a B2 sized vector of mag-sign values, Euclid algorithm
template<typename T> static T gcf(const T* group){
    static_assert(std::is_integral<T>() && std::is_unsigned<T>(), "Only unsigned integer types allowed");
    // Work with absolute values
    int sz = 0;
    T v[B2]; // No need to initialize
    for (int i = 0; i < B2; i++) {
        v[sz] = magsabs(group[i]);
        sz += 0 < v[sz]; // Skip the zeros
    }
    do {
        // minimum value and location
        T m = v[0];
        int j = 0;
        for (int i = 1; i < sz; i++)
            if (m > v[i])
                m = v[j = i];
        if (1 == m)
            return 1; // common factor
        // Force the min at v[0]
        v[j] = v[0];
        v[0] = m;
        for (int i = j = 1; i < sz; i++) {
            v[j] = v[i] % m;
            j += 0 < v[j]; // Skip the zeros
        }
        sz = j; // Never zero
    } while (sz > 1);
    return v[0]; // common factor > 1
}

// Computed encoding with three codeword lenghts, used for higher rungs
// No conditionals, computes all three values and picks one by masking with the condition
// It is faster than similar code with conditions because the calculations for the three lines get interleaved
// The "(~0ull * (1 & <cond>))" is to show the compiler that it is a mask operation
static std::pair<size_t, uint64_t> qb3csz(uint64_t val, size_t rung) {
    assert(rung > 1); // Works for rungs 2+
    uint64_t nxt = (val >> (rung - 1)) & 1;
    uint64_t top = val >> rung;
    uint64_t tb = 1ull << rung;
    return std::make_pair<size_t, uint64_t>(rung + top + (top | nxt),
        + ((~0ull * (1 & top)) & (((val ^ tb) << 2) | 0b11))                // 1 x LONG     -> _11
        + ((~0ull * (1 & ~(top | nxt))) & (val << 1))                       // 0 0 SHORT    -> _x0
        + ((~0ull * (1 & (~top & nxt))) & ((((val << 1) ^ tb) << 1) + 1))); // 0 1 NOMINAL  -> _01
}

// Single value QB3 encode, possibly using tables, works for all rungs
static std::pair<size_t, uint64_t> qb3csztbl(uint64_t val, size_t rung) {
    if ((sizeof(CRG) / sizeof(*CRG)) > rung) {
        auto cs = CRG[rung][val];
        return std::make_pair<size_t, uint64_t>(cs >> 12, cs & TBLMASK);
    }
    return qb3csz(val, rung);
}

// only encode the group entries, not the rung switch
// maxval is used to choose the rung for encoding
// If abits > 0, the accumulator is also pushed into the stream
template <typename T>
static void groupencode(T group[B2], T maxval, oBits& s,
    uint64_t acc = 0, size_t abits = 0)
{
    assert(abits <= 64);
    if (abits > 8) { // Just in case, a rung switch is 8 bits at most
        s.push(acc, abits);
        acc = abits = 0;
    }
    const size_t rung = topbit(maxval | 1); // Force at least one bit set
    if (0 == rung) { // only 1s and 0s, rung is -1 or 0
        acc |= static_cast<uint64_t>(maxval) << abits++;
        if (0 != maxval)
            for (int i = 0; i < B2; i++)
                acc |= static_cast<uint64_t>(group[i]) << abits++;
        s.push(acc, abits);
        return;
    }
    // Flip the last set rung bit if the rung bit sequence is a step down
    // At least one rung bit has to be set, so it can't return 0
    auto stepp = step(group, rung);
    assert(stepp > 0); // At least one rung bit should be set
    if (stepp <= B2)
        group[stepp - 1] ^= static_cast<T>(1ull << rung);
    if (6 > rung) { // Half of the group fits in 64 bits
        auto t = CRG[rung];
        for (size_t i = 0; i < B2 / 2; i++) {
            acc |= (TBLMASK & t[group[i]]) << abits;
            abits += t[group[i]] >> 12;
        }
        // No need to push accumulator at rung 1 and sometimes 2
        if (rung != 1 && (rung != 2 || abits > 32)) {
            s.push(acc, abits);
            acc = abits = 0;
        }

        for (size_t i = B2 / 2; i < B2; i++) {
            acc |= (TBLMASK & t[group[i]]) << abits;
            abits += t[group[i]] >> 12;
        }
        s.push(acc, abits);
        return;
    }
    // Last part of table encoding, rung 6-7
    // Encoded data fits in 256 bits, 4 way interleaved
    if ((sizeof(CRG) / sizeof(*CRG)) > rung) {
        auto t = CRG[rung];
        uint64_t a[4] = {acc, 0, 0, 0};
        size_t asz[4] = {abits, 0, 0, 0};
        for (size_t i = 0; i < B; i++) {
            for (size_t j = 0; j < B; j++) {
                auto v = t[group[j * B + i]];
                a[j] |= (TBLMASK & v) << asz[j];
                asz[j] += v >> 12;
            }
        }
        for (size_t i = 0; i < B; i++)
            s.push(a[i], asz[i]);
        return;
    }
    // Computed encoding, slower, works for rung > 1
    if (1 < sizeof(T)) { // This vanishes in 8 bit mode
        if (sizeof(T) < 8 || rung < 31) { // low rung, can reuse acc
            for (int i = 0; i < B2; i++) {
                auto p = qb3csz(group[i], rung);
                if (p.first + abits > 64) {
                    s.push(acc, abits);
                    acc = abits = 0;
                }
                acc |= p.second << abits;
                abits += p.first;
            }
            s.push(acc, abits);
        }
        else { // sizeof(T) == 8
            s.push(acc, abits);
            if (rung < 63) { // high rung, no overflow
                for (int i = 0; i < B2; i++) {
                    auto p = qb3csz(group[i], rung);
                    s.push(p.second, p.first);
                }
            }
            else { // top rung, might overflow
                for (int i = 0; i < B2; i++) {
                    auto p = qb3csz(group[i], rung);
                    size_t ovf = p.first & (p.first >> 6); // overflow flag
                    s.push(p.second, p.first ^ ovf); // changes 65 in 64
                    if (ovf)
                        s.push(1ull & (static_cast<uint64_t>(group[i]) >> 62), ovf);
                }
            }
        }
    }
}

template<typename T> constexpr uint16_t SIGNAL = sizeof(T) == 1 ? 0x5017 : sizeof(T) == 2 ? 0x6037 : sizeof(T) == 4 ? 0x7077 : 0x80f7;
template<typename T> constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
template<typename T> constexpr uint64_t UBMASK = (1ull << UBITS<T>) - 1;
template<typename T> uint16_t const *CSW = sizeof(T) == 1 ? csw3 : sizeof(T) == 2 ? csw4 : sizeof(T) == 4 ? csw5 : csw6;

// Base QB3 group encode with code switch, returns encoded size
template <typename T = uint8_t> 
static void groupencode(T group[B2], T maxval, size_t oldrung, oBits& s) {
    uint64_t acc = CSW<T>[(topbit(maxval | 1) - oldrung) & UBMASK<T>];
    groupencode(group, maxval, s, acc & TBLMASK, static_cast<size_t>(acc >> 12));
}

// Group encode with cf
template <typename T = uint8_t>
static void cfgenc(oBits &bits, T group[B2], T cf, size_t oldrung) {
    // Signal as switch to same rung, max-positive value, by UBITS
    uint64_t acc = SIGNAL<T> & TBLMASK;
    size_t abits = SIGNAL<T> >> 12;
    // divide group values by CF and find the new maxvalue
    T maxval = 0;
    for (size_t i = 0; i < B2; i++) if (group[i])
        maxval = std::max(maxval, group[i] = T(((magsabs(group[i]) / cf) << 1) - (group[i] & 1)));
    cf -= 2; // Bias down, 0 and 1 are not used
    auto trung = topbit(maxval | 1); // cf mode rung
    auto cfrung = topbit(cf | 1); // rung for cf-2 value
    // Encode the trung, with or without switch
    // Use the wrong way switch for in-band
    auto cs = CSW<T>[(trung - oldrung) & UBMASK<T>];
    if ((cs >> 12) == 1) // Would be no-switch, use signal instead, it decodes to delta of zero
        cs = SIGNAL<T>;
    // When trung is only somewhat larger than cfrung encode cf at same rung as data
    if (trung >= cfrung && (trung < (cfrung + UBITS<T>) || 0 == cfrung)) {
        acc |= (cs & TBLMASK) << abits;
        abits += cs >> 12;
        if (trung == 0) { // Special encoding for single bit
            // maxval can't be zero, so we don't use the all-zeros flag
            // But we do need to save the CF bit
            acc |= static_cast<uint64_t>(cf) << abits++;
            // And the group bits
            for (int i = 0; i < B2; i++)
                acc |= static_cast<uint64_t>(group[i]) << abits++;
            bits.push(acc, abits);
            return;
        }
        // Push the accumulator and the cf encoding, 0 < rung < 63
        auto p = qb3csztbl(cf, trung);
        bits.push(acc, abits);
        acc = p.second;
        abits = p.first;
    }
    else { // CF needs a different rung than the group, so the change is never 0
        // First, encode trung using code-switch with the change bit cleared
        acc |= (cs & (TBLMASK - 1)) << abits;
        abits += cs >> 12;
        // Then encode cfrung, using code-switch from trung, 
        // skip the change bit, since rung will always be different
        // cfrung - trung is never 0
        cs = CSW<T>[(cfrung - trung) & UBMASK<T>];
        acc |= (cs & (TBLMASK - 1)) << (abits - 1);
        abits += static_cast<size_t>(cs >> 12) - 1;
        if (cfrung > 1) {
            auto p = qb3csztbl(cf ^ (1ull << cfrung), cfrung - 1); // Can't overflow
            bits.push(acc, abits);
            acc = p.second;
            abits = p.first;
        }
        else { // single bit, there is enough space, cfrung 0 or 1, save only the bottom bit
            acc |= static_cast<uint64_t>(cf - static_cast<T>(cfrung * 2)) << abits++;
        }
        if (0 == trung) { // encode the group bits directly, saves the flag bit
            bits.push(acc, abits);
            acc = 0;
            for (int i= 0; i < B2; i++)
                acc |= static_cast<uint64_t>(group[i]) << i;
            bits.push(acc, B2);
            return;
        }
    }
    groupencode(group, maxval, bits, acc, abits);
}

// Round to Zero Division, no overflow
template<typename T>
static T rto0div(T x, T y) {
    static_assert(std::is_integral<T>(), "Integer types only");
    T r = x / y, m = x % y;
    y = (y >> 1);
    return r + (!(x < 0) & (m > y)) - ((x < 0) & ((m + y) < 0));
}

// Round from Zero Division, no overflow
template<typename T>
static T rfr0div(T x, T y) {
    static_assert(std::is_integral<T>(), "Integer types only");
    T r = x / y, m = x % y;
    y = (y >> 1) + (y & 1);
    return r + (!(x < 0) & (m >= y)) - ((x < 0) & ((m + y) <= 0));
}

// Best block traversal order in most cases
static const uint8_t xlut[16] = { 0, 1, 0, 1, 2, 3, 2, 3, 0, 1, 0, 1, 2, 3, 2, 3 };
static const uint8_t ylut[16] = { 0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 3, 3, 2, 2, 3, 3 };

// Only basic encoding
template<typename T>
static int encode_fast(const T* image, oBits& s, encs &info)
{
    static_assert(std::is_integral<T>() && std::is_unsigned<T>(), "Only unsigned integer types allowed");
    const size_t xsize(info.xsize), ysize(info.ysize), bands(info.nbands), * cband(info.cband);
    if (xsize == 0 || xsize > 0x10000ull || ysize == 0 || ysize > 0x10000ull || 0 == bands)
        return 1;
    // Check band mapping
    for (size_t c = 0; c < bands; c++)
        if (cband[c] >= bands)
            return 2; // Band mapping error
    // Running code length
    size_t runbits[QB3_MAXBANDS];
    // Previous value, per band
    T prev[QB3_MAXBANDS];
    // Initialize state
    for (size_t c = 0; c < bands; c++) {
        runbits[c] = info.runbits[c];
        prev[c] = static_cast<T>(info.prev[c]);
    }
    size_t offsets[B2];
    for (size_t i = 0; i < B2; i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;
    T group[B2];
    for (size_t y = 0; y < ysize; y += B) {
        // If the last row is partial, roll it up
        if (y + B > ysize)
            y = ysize - B;
        for (size_t x = 0; x < xsize; x += B) {
            // If the last column is partial, move it left
            if (x + B > xsize)
                x = xsize - B;                
            const size_t loc = (y * xsize + x) * bands; // Top-left pixel address
            for (size_t c = 0; c < bands; c++) { // blocks are band interleaved
                T maxval(0); // Maximum mag-sign value within this group
                // Collect the block for this band, convert to running delta mag-sign
                auto prv = prev[c];
                // Use separate loop for basebands to avoid a test inside the hot loop
                if (c != cband[c]) {
                    for (size_t i = 0; i < B2; i++) {
                        T g = image[loc + c + offsets[i]] - image[loc + cband[c] + offsets[i]];
                        prv += g -= prv;
                        group[i] = mags(g);
                        maxval = std::max(maxval, mags(g));
                    }
                }
                else { // baseband
                    for (size_t i = 0; i < B2; i++) {
                        T g = image[loc + c + offsets[i]];
                        prv += g -= prv;
                        group[i] = mags(g);
                        maxval = std::max(maxval, mags(g));
                    }
                }
                prev[c] = prv;
                groupencode(group, maxval, runbits[c], s);
                runbits[c] = topbit(maxval | 1);
            }
        }
    }
    // Save the state
    for (size_t c = 0; c < bands; c++) {
        info.prev[c] = static_cast<size_t>(prev[c]);
        info.runbits[c] = runbits[c];
    }
    return 0;
}

// Returns error code or 0 if success
// TODO: Error code mapping
template <typename T = uint8_t>
static int encode_best(const T *image, oBits& s, encs &info)
{
    static_assert(std::is_integral<T>() && std::is_unsigned<T>(), "Only unsigned integer types allowed");
    const size_t xsize(info.xsize), ysize(info.ysize), bands(info.nbands), * cband(info.cband);
    if (xsize == 0 || xsize > 0x10000ull || ysize == 0 || ysize > 0x10000ull || 0 == bands)
        return 1;
    // Check band mapping
    for (size_t c = 0; c < bands; c++)
        if (cband[c] >= bands)
            return 2; // Band mapping error
    // Running code length, start with nominal value
    size_t runbits[QB3_MAXBANDS];
    // Previous value, per band
    T prev[QB3_MAXBANDS];
    for (size_t c = 0; c < bands; c++) {
        runbits[c] = info.runbits[c];
        prev[c] = static_cast<T>(info.prev[c]);
    }
    size_t offsets[B2];
    for (size_t i = 0; i < B2; i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;
    T group[B2]; // Current 2D group to encode, as array
    for (size_t y = 0; y < ysize; y += B) {
        // If the last row is partial, roll it up
        if (y + B > ysize)
            y = ysize - B;
        for (size_t x = 0; x < xsize; x += B) {
            // If the last column is partial, move it left
            if (x + B > xsize)
                x = xsize - B;
            size_t loc = (y * xsize + x) * bands; // Top-left pixel address
            for (size_t c = 0; c < bands; c++) { // blocks are always band interleaved
                T maxval(0); // Maximum mag-sign value within this group
                // Collect the block for this band, convert to running delta mag-sign
                auto prv = prev[c];
                if (c != cband[c]) {
                    for (size_t i = 0; i < B2; i++) {
                        T g = image[loc + c + offsets[i]] - image[loc + cband[c] + offsets[i]];
                        prv += g -= prv;
                        group[i] = mags(g);
                        maxval = std::max(maxval, mags(g));
                    }
                }
                else {
                    for (size_t i = 0; i < B2; i++) {
                        T g = image[loc + c + offsets[i]];
                        prv += g -= prv;
                        group[i] = mags(g);
                        maxval = std::max(maxval, mags(g));
                    }
                }
                prev[c] = prv;
                auto oldrung = runbits[c];
                const size_t rung = topbit(maxval | 1);
                runbits[c] = rung;
                if (0 == rung) { // only 1s and 0s, rung is -1 or 0, no point in trying cf
                    uint64_t acc = CSW<T>[(rung - oldrung) & UBMASK<T>];
                    size_t abits = acc >> 12;
                    acc &= 0xffull;
                    acc |= static_cast<uint64_t>(maxval) << abits++; // Add the all-zero flag
                    if (0 != maxval)
                        for (size_t i = 0; i < B2; i++)
                            acc |= static_cast<uint64_t>(group[i]) << abits++;
                    s.push(acc, abits);
                    continue;
                }
                auto cf = gcf(group);
                if (cf < 2)
                    groupencode(group, maxval, oldrung, s);
                else // Always smaller in cf encoding
                    cfgenc(s, group, cf, oldrung);
            }
        }
    }
    // Save the state
    for (size_t c = 0; c < bands; c++) {
        info.prev[c] = static_cast<size_t>(prev[c]);
        info.runbits[c] = runbits[c];
    }
    return 0;
}
} // namespace
