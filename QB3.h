#pragma once
#include <vector>
#include <cinttypes>
#include <limits>
#include <functional>
#include <intrin.h>
#include <cassert>

namespace QB3 {
    // Masks, from 0 to 64 rung
#define M(v) (~0ull >> (64 - (v)))
// A row of 8 masks, starting with mask(n)
#define R(n) M(n), M(n + 1), M(n + 2), M(n + 3), M(n + 4), M(n + 5), M(n + 6), M(n + 7)
    const uint64_t mask[] = { 0, R(1), R(9), R(17), R(25), R(33), R(41), R(49), R(57) };
#undef R
#undef M

// rank of top set bit, result is undefined for val == 0
static size_t bsr(uint64_t val) {
#if defined(_WIN32)
    return 63 - __lzcnt64(val);
#else
#if defined(__GNUC__)
    return 63 - __builtin_clzll(val);
#else
    // no builtin is available
    unsigned r = 0;
    while (val >>= 1)
        r++;
    return r;
#endif
#endif
}

// Greater common denominator
template<typename T>
T gcd(const std::vector<T>& vals) {
    if (vals.empty())
        return 0;
    auto v(vals); // Work on a copy
    auto vb(v.begin()), ve(v.end());
    for (;;) {
        sort(vb, ve);
        while (!*vb)
            if (++vb == ve)
                return 0;
        auto m(*vb);
        if (vb + 1 == ve)
            return m;
        for (auto vi(vb + 1); vi < ve; vi++)
            *vi %= m;
    }
    // Not reached
    return 0;
}

// Convert from mag-sign to absolute
template<typename T>
inline T revs(T val) {
    return (val >> 1) + (val & 1);
}

// return greatest common factor
// T is always unsigned
template<typename T>
T gcode(const std::vector<T>& vals) {

    // heap of absolute values
    std::vector<T> v;
    v.reserve(vals.size());
    for (auto val : vals) {
        // ignore the zeros
        if (val == 0) continue;
        // if a value is 1 or -1, the only common factor will be 1
        if (val < 3) return 1;
        v.push_back(revs(val));
        push_heap(v.begin(), v.end(), std::greater<T>());
    }

    if (v.empty() || v.front() < 3)
        return 1;

    do {
        pop_heap(v.begin(), v.end(), std::greater<T>());
        const T m(v.back()); // never less than 2
        v.pop_back();
        for (auto& t : v) t %= m;
        v.push_back(m); // v is never empty
        make_heap(v.begin(), v.end(), std::greater<T>());
        while (v.size() && v.front() == 0) {
            pop_heap(v.begin(), v.end(), std::greater<T>());
            v.pop_back();
        }
        if (1 == v.front())
            return 1;
    } while (v.size() > 1);
    return v.front();
}

//
// Delta and sign reorder
// All these templates work only for T unsigned, integer, 2s complement types
// Even though T is unsigned, it is considered signed, that's the whole point!
//


// Encode integers as absolute magnitude and sign, so the bit 0 is the sign.
// This encoding has the top rung always zero, regardless of sign
// To keep the range exactly the same as two's complement, the magnitude of negative values is biased down by one (no negative zero)


// Change to mag-sign without conditionals, fast
template<typename T>
static T mags(T v) {
    return (std::numeric_limits<T>::max() * (v >> (8 * sizeof(T) - 1))) ^ (v << 1);
}

// Undo mag-sign without conditionals, faster
template<typename T>
static T smag(T v) {
    return (std::numeric_limits<T>::max() * (v & 1)) ^ (v >> 1);
}

// Convert a sequence to mag-sign delta
template<typename T, size_t B2 = 16>
static T dsign(T *v, T pred) {
    static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
        "Only works for unsigned integral types");
    for (int i = 0; i < B2; i++) {
        pred += v[i] -= pred; // or std::swap(v[i], pred); v[i] = pred - v[i] 
        v[i] = mags(v[i]);
    }
    return pred;
}

// Reverse mag-sign and delta
template<typename T, size_t B2 = 16>
static T undsign(T *v, T pred) {
    for (int i = 0; i < B2; i++)
        v[i] = pred += smag(v[i]);
    return pred;
}

// If the front of v contains values higher than m and the rest are under or equal 
// return the position of the last value higher than m.
// Otherwise return the one after the last position
template<typename T>
static int step(const std::vector<T>& v, const T m = 0) {
    const int sz = static_cast<int>(v.size());
    int i = 0;
    while (i < sz && v[i] > m)
        i++;
    int s = i; // Bit position of last high value
    while (i < sz && v[i] <= m)
        i++;
    // If the loop completed then it's a step vector
    return (i == sz) ? s : sz;
}

// Block size should be 8, for noisy images 4 is better
// 2 generates too much overhead
// 16 might work for slow varying inputs
// which would need the lookup tables extended

// Traversal order tables
static const uint8_t xlut[16] = {0, 1, 0, 1, 2, 3, 2, 3, 0, 1, 0, 1, 2, 3, 2, 3};
static const uint8_t ylut[16] = {0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 3, 3, 2, 2, 3, 3};

// QB3 encoding tables for byte values. Rung 0 and 1 are special
static const uint16_t crg2[] = {0x2002, 0x2003, 0x3001, 0x3005, 0x4000, 0x4004, 0x4008, 0x400c};
static const uint16_t crg3[] = {0x3004, 0x3005, 0x3006, 0x3007, 0x4002, 0x400a, 0x4003, 0x400b, 0x5000, 0x5008, 0x5010, 0x5018,
0x5001, 0x5009, 0x5011, 0x5019};
static const uint16_t crg4[] = {0x4008, 0x4009, 0x400a, 0x400b, 0x400c, 0x400d, 0x400e, 0x400f, 0x5004, 0x5014, 0x5005, 0x5015,
0x5006, 0x5016, 0x5007, 0x5017, 0x6000, 0x6010, 0x6020, 0x6030, 0x6001, 0x6011, 0x6021, 0x6031, 0x6002, 0x6012, 0x6022, 0x6032,
0x6003, 0x6013, 0x6023, 0x6033};
static const uint16_t crg5[] = {0x5010, 0x5011, 0x5012, 0x5013, 0x5014, 0x5015, 0x5016, 0x5017, 0x5018, 0x5019, 0x501a, 0x501b,
0x501c, 0x501d, 0x501e, 0x501f, 0x6008, 0x6028, 0x6009, 0x6029, 0x600a, 0x602a, 0x600b, 0x602b, 0x600c, 0x602c, 0x600d, 0x602d,
0x600e, 0x602e, 0x600f, 0x602f, 0x7000, 0x7020, 0x7040, 0x7060, 0x7001, 0x7021, 0x7041, 0x7061, 0x7002, 0x7022, 0x7042, 0x7062,
0x7003, 0x7023, 0x7043, 0x7063, 0x7004, 0x7024, 0x7044, 0x7064, 0x7005, 0x7025, 0x7045, 0x7065, 0x7006, 0x7026, 0x7046, 0x7066,
0x7007, 0x7027, 0x7047, 0x7067};
static const uint16_t crg6[] = {0x6020, 0x6021, 0x6022, 0x6023, 0x6024, 0x6025, 0x6026, 0x6027, 0x6028, 0x6029, 0x602a, 0x602b,
0x602c, 0x602d, 0x602e, 0x602f, 0x6030, 0x6031, 0x6032, 0x6033, 0x6034, 0x6035, 0x6036, 0x6037, 0x6038, 0x6039, 0x603a, 0x603b,
0x603c, 0x603d, 0x603e, 0x603f, 0x7010, 0x7050, 0x7011, 0x7051, 0x7012, 0x7052, 0x7013, 0x7053, 0x7014, 0x7054, 0x7015, 0x7055,
0x7016, 0x7056, 0x7017, 0x7057, 0x7018, 0x7058, 0x7019, 0x7059, 0x701a, 0x705a, 0x701b, 0x705b, 0x701c, 0x705c, 0x701d, 0x705d,
0x701e, 0x705e, 0x701f, 0x705f, 0x8000, 0x8040, 0x8080, 0x80c0, 0x8001, 0x8041, 0x8081, 0x80c1, 0x8002, 0x8042, 0x8082, 0x80c2,
0x8003, 0x8043, 0x8083, 0x80c3, 0x8004, 0x8044, 0x8084, 0x80c4, 0x8005, 0x8045, 0x8085, 0x80c5, 0x8006, 0x8046, 0x8086, 0x80c6,
0x8007, 0x8047, 0x8087, 0x80c7, 0x8008, 0x8048, 0x8088, 0x80c8, 0x8009, 0x8049, 0x8089, 0x80c9, 0x800a, 0x804a, 0x808a, 0x80ca,
0x800b, 0x804b, 0x808b, 0x80cb, 0x800c, 0x804c, 0x808c, 0x80cc, 0x800d, 0x804d, 0x808d, 0x80cd, 0x800e, 0x804e, 0x808e, 0x80ce,
0x800f, 0x804f, 0x808f, 0x80cf};
static const uint16_t crg7[] = {0x7040, 0x7041, 0x7042, 0x7043, 0x7044, 0x7045, 0x7046, 0x7047, 0x7048, 0x7049, 0x704a, 0x704b,
0x704c, 0x704d, 0x704e, 0x704f, 0x7050, 0x7051, 0x7052, 0x7053, 0x7054, 0x7055, 0x7056, 0x7057, 0x7058, 0x7059, 0x705a, 0x705b,
0x705c, 0x705d, 0x705e, 0x705f, 0x7060, 0x7061, 0x7062, 0x7063, 0x7064, 0x7065, 0x7066, 0x7067, 0x7068, 0x7069, 0x706a, 0x706b,
0x706c, 0x706d, 0x706e, 0x706f, 0x7070, 0x7071, 0x7072, 0x7073, 0x7074, 0x7075, 0x7076, 0x7077, 0x7078, 0x7079, 0x707a, 0x707b,
0x707c, 0x707d, 0x707e, 0x707f, 0x8020, 0x80a0, 0x8021, 0x80a1, 0x8022, 0x80a2, 0x8023, 0x80a3, 0x8024, 0x80a4, 0x8025, 0x80a5,
0x8026, 0x80a6, 0x8027, 0x80a7, 0x8028, 0x80a8, 0x8029, 0x80a9, 0x802a, 0x80aa, 0x802b, 0x80ab, 0x802c, 0x80ac, 0x802d, 0x80ad,
0x802e, 0x80ae, 0x802f, 0x80af, 0x8030, 0x80b0, 0x8031, 0x80b1, 0x8032, 0x80b2, 0x8033, 0x80b3, 0x8034, 0x80b4, 0x8035, 0x80b5,
0x8036, 0x80b6, 0x8037, 0x80b7, 0x8038, 0x80b8, 0x8039, 0x80b9, 0x803a, 0x80ba, 0x803b, 0x80bb, 0x803c, 0x80bc, 0x803d, 0x80bd,
0x803e, 0x80be, 0x803f, 0x80bf, 0x9000, 0x9080, 0x9100, 0x9180, 0x9001, 0x9081, 0x9101, 0x9181, 0x9002, 0x9082, 0x9102, 0x9182,
0x9003, 0x9083, 0x9103, 0x9183, 0x9004, 0x9084, 0x9104, 0x9184, 0x9005, 0x9085, 0x9105, 0x9185, 0x9006, 0x9086, 0x9106, 0x9186,
0x9007, 0x9087, 0x9107, 0x9187, 0x9008, 0x9088, 0x9108, 0x9188, 0x9009, 0x9089, 0x9109, 0x9189, 0x900a, 0x908a, 0x910a, 0x918a,
0x900b, 0x908b, 0x910b, 0x918b, 0x900c, 0x908c, 0x910c, 0x918c, 0x900d, 0x908d, 0x910d, 0x918d, 0x900e, 0x908e, 0x910e, 0x918e,
0x900f, 0x908f, 0x910f, 0x918f, 0x9010, 0x9090, 0x9110, 0x9190, 0x9011, 0x9091, 0x9111, 0x9191, 0x9012, 0x9092, 0x9112, 0x9192,
0x9013, 0x9093, 0x9113, 0x9193, 0x9014, 0x9094, 0x9114, 0x9194, 0x9015, 0x9095, 0x9115, 0x9195, 0x9016, 0x9096, 0x9116, 0x9196,
0x9017, 0x9097, 0x9117, 0x9197, 0x9018, 0x9098, 0x9118, 0x9198, 0x9019, 0x9099, 0x9119, 0x9199, 0x901a, 0x909a, 0x911a, 0x919a,
0x901b, 0x909b, 0x911b, 0x919b, 0x901c, 0x909c, 0x911c, 0x919c, 0x901d, 0x909d, 0x911d, 0x919d, 0x901e, 0x909e, 0x911e, 0x919e,
0x901f, 0x909f, 0x911f, 0x919f};

static const uint16_t *CRG[] = {nullptr, nullptr, crg2, crg3, crg4, crg5, crg6, crg7};

// Encoding with three codeword lenghts
template <typename T = uint8_t, size_t B = 4>
std::vector<uint8_t> encode(const std::vector<T>& image,
    size_t xsize, size_t ysize, int mb = 1)
{
    std::vector<uint8_t> result;
    result.reserve(image.size() * sizeof(T));
    Bitstream s(result);
    const size_t bands = image.size() / xsize / ysize;
    assert(image.size() == xsize * ysize * bands);
    assert(0 == xsize % B && 0 == ysize % B);

    // Unit size bit length
    constexpr size_t UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    // Elements in a group
    constexpr size_t B2(B * B);

    // Running code length, start with nominal value
    std::vector<size_t> runbits(bands, sizeof(T) * 8 - 1);
    std::vector<T> prev(bands, 0u);      // Previous value, per band
    T group[B2];  // Current 2D group to encode, as array
    size_t offsets[B2];
    for (size_t i = 0; i < B2; i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;

    //for (size_t y = 0; (y + B) <= ysize; y += B) {
    //    for (size_t x = 0; (x + B) <= xsize; x += B) {
    for (size_t y = 0; y < ysize; y += B) {
        for (size_t x = 0; x < xsize; x += B) {
            size_t loc = (y * xsize + x) * bands; // Top-left pixel address
            for (size_t c = 0; c < bands; c++) { // blocks are band interleaved

                // Collect the block for this band
                if (mb >= 0 && mb != c) {
                    for (size_t i = 0; i < B2; i++)
                        group[i] = image[loc + offsets[i] + c] - image[loc + offsets[i] + mb];
                }
                else {
                    for (size_t i = 0; i < B2; i++)
                        group[i] = image[loc + offsets[i] + c];
                }

                // Delta in low sign group encode
                prev[c] = dsign(group, prev[c]);
                const uint64_t maxval = *std::max_element(group, group + B2);

                uint64_t acc = 0;
                size_t abits = 0;

                if (maxval < 2) { // only 1 and 0, rung -1 or 0
                    abits = 2; // assume no rung change
                    acc = maxval << 1; // 00 for rung -1, 01 for rung 0
                    if (0 != runbits[c]) { // rung change
                        acc = (acc << UBITS) + 1;
                        abits = UBITS + 2;
                        runbits[c] = 0;
                    }
                    if (0 == maxval) { // rung -1, we're done
                        s.push(acc, abits);
                        continue;
                    }
                    // rung 0
                    for (auto& v : group)
                        acc |= v << abits++;
                    s.push(acc, abits);
                    continue;
                }

                // Top set bit, maxval > 1
                const size_t rung = bsr(maxval);

                // Rung change, if needed
                if (runbits[c] == rung)
                    s.push(0u, 1); // Same rung single zero bit
                else {
                    //auto newcode = rswitch<T>(rung - runbits[c]);
                    s.push((rung << 1) + 1, UBITS + 1); // change bit + len on UBITS
                    runbits[c] = rung;
                }

                if (1 == rung) { // 2 bit nominal size, doesn't always have 2 detection rung
                    const static size_t   c1sizes[] = { 1, 2, 3, 3 };
                    const static uint64_t c1codes[] = { 0, 3, 1, 5 };
                    for (auto it : group) {
                        acc |= c1codes[it] << abits;
                        abits += c1sizes[it];
                    }
                    s.push(acc, abits);
                    continue;
                }

                if (2 == rung) { // Encoded data size fits in 64 bits
                    for (auto it : group) {
                        acc |= (0xfffull & crg2[it]) << abits;
                        abits += crg2[it] >> 12;
                    }
                    s.push(acc, abits);
                    continue;
                }

                if (7 > rung) { // Encoded data fits in 128 bits, unroll by 2
                    auto table = CRG[rung];
                    int i = 0;
                    for (; i < B2 / 2; i++) {
                        auto v = table[group[i]];
                        acc |= (0xfffull & v) << abits;
                        abits += v >> 12;
                    }
                    s.push(acc, abits);
                    acc = abits = 0;
                    for (; i < B2; i++) {
                        auto v = table[group[i]];
                        acc |= (0xfffull & v) << abits;
                        abits += v >> 12;
                    }
                    s.push(acc, abits);
                    continue;
                }

                if (7 == rung) { // Encoded data fits in 128 bits, unroll by 2
                    auto table = CRG[rung];
                    int i = 0;
                    for (; i < B2 / 4; i++) {
                        auto v = table[group[i]];
                        acc |= (0xfffull & v) << abits;
                        abits += v >> 12;
                    }
                    s.push(acc, abits);
                    acc = abits = 0;
                    for (; i < B2 / 2; i++) {
                        auto v = table[group[i]];
                        acc |= (0xfffull & v) << abits;
                        abits += v >> 12;
                    }
                    s.push(acc, abits);
                    acc = abits = 0;
                    for (; i < B2 * 3 / 4; i++) {
                        auto v = table[group[i]];
                        acc |= (0xfffull & v) << abits;
                        abits += v >> 12;
                    }
                    s.push(acc, abits);
                    acc = abits = 0;
                    for (; i < B2; i++) {
                        auto v = table[group[i]];
                        acc |= (0xfffull & v) << abits;
                        abits += v >> 12;
                    }
                    s.push(acc, abits);
                    continue;
                }

                // Generic three length encoding
                // The bottom "rung" bits start with two detection bits and are read as a group
                for (uint64_t val : group) {
                    if (val <= mask[rung - 1]) { // First quarter, short codewords
                        val |= 1ull << (rung - 1);
                        s.push(val, rung); // Starts with 1
                    }
                    else if (val <= mask[rung]) { // Second quarter, nominal size
                        val = (val >> 1) | ((val & 1) << rung);
                        s.push(val, rung + 1); // starts with 01, rotated right one bit
                    }
                    else { // last half, least likely, long codewords
                        val &= mask[rung];
                        if (sizeof(T) == 8 && rung == 63) {
                            // can't push 65 rung in a single call
                            s.push(val >> 2, rung);
                            s.push(val & 0b11, 2);
                        } 
                        else {
                            val = (val >> 2) | ((val & 0b11) << rung);
                            s.push(val, rung + 2); // starts with 00, rotated two rung
                        }
                    }
                }
            }
        }
    }
    return result;
}

template<typename T = uint8_t, size_t B = 4>
std::vector<T> decode(std::vector<uint8_t>& src, size_t xsize, size_t ysize, 
    size_t bands, int mb = 1)
{
    std::vector<T> image(xsize * ysize * bands);
    Bitstream s(src);
    std::vector<T> prev(bands, 0);
    // Elements in a group
    constexpr size_t B2(B * B);
    std::vector<T> group(B2);

    // Unit size bit length
    const constexpr int UBITS = sizeof(T) == 1 ? 3 : sizeof(T) == 2 ? 4 : sizeof(T) == 4 ? 5 : 6;
    std::vector<size_t> runbits(bands, sizeof(T) * 8 - 1);

    std::vector<size_t> offsets(B2);
    for (size_t i = 0; i < B2; i++)
        offsets[i] = (xsize * ylut[i] + xlut[i]) * bands;

    for (size_t y = 0; (y + B) <= ysize; y += B) {
        for (size_t x = 0; (x + B) <= xsize; x += B) {
            size_t loc = (y * xsize + x) * bands;
            for (int c = 0; c < bands; c++) {
                uint64_t bits = runbits[c];
                if (0 != s.get()) { // The rung change flag
                    s.pull(runbits[c], UBITS);
                    bits = runbits[c];
                }
                uint64_t val;
                if (0 == bits) { // 0 or 1
                    if (0 == s.get()) // All 0s
                        fill(group.begin(), group.end(), 0);
                    else { // 0 and 1s
                        s.pull(val, group.size());
                        for (int i = 0; i < B2; i++) {
                            group[i] = val & 1;
                            val >>= 1;
                        }
                    }
                }
                else if (1 == bits) { // 2 rung nominal, could be a single bit
                    for (auto& it : group)
                        if ((it = static_cast<T>(s.get())))
                            if (!(it = static_cast<T>(s.get())))
                                it = static_cast<T>(0b10 + s.get());
                }
                else { // triple length
                    for (auto& it : group) {
                        s.pull(it, bits);
                        if (it > mask[bits - 1]) // Starts with 1x
                            it &= mask[bits - 1];
                        else if (it > mask[bits - 2]) // Starts with 01
                            it = static_cast<T>(s.get() + (it << 1));
                        else { // starts with 00
                            s.pull(val, 2);
                            it = static_cast<T>((1ull << bits) + (it << 2) + val);
                        }
                    }
                }
                prev[c] = undsign(group.data(), prev[c]);
                for (size_t i = 0; i < group.size(); i++)
                    image[loc + c + offsets[i]] = group[i];
            }
            for (int c = 0; c < bands; c++)
                if (mb >= 0 && mb != c)
                    for (size_t i = 0; i < group.size(); i++)
                        image[loc + c + offsets[i]] += image[loc + mb + offsets[i]];
        }
    }
    return image;
}

} // Namespace
