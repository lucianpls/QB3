#include "bmap.h"
#include <iostream>
#include <algorithm>

const uint8_t Bitstream::mask[9] = {
    0,
    0b1,
    0b11,
    0b111,
    0b1111,
    0b11111,
    0b111111,
    0b1111111,
    0b11111111
};

BMap::BMap(int x, int y) : _x(x), _y(y), _lw((x + 7) / 8) {
    v.assign(_lw * ((y + 7) / 8), ~0); // All data
}

int rlen(const uint8_t* ch, size_t mlen) {
    static const size_t MRUN = 0x300 + 0xffff;
    mlen = std::min(mlen, MRUN);
    const uint8_t* v = ch;
    while (mlen-- && *ch == *v) v++;
    return static_cast<int>(v - ch);
}

#define CODE 0xC5
void RLE(std::vector<uint8_t>& v, std::vector<uint8_t>& result) {
    // Do a byte RLE on v
    std::vector<uint8_t> tmp;

    size_t len = 0;
    while (len < v.size()) {
        int l = rlen(v.data() + len, v.size() - len);
        uint8_t c = v[len];
        if (l < 4) {
            tmp.push_back(c);
            if (CODE == c)
                tmp.push_back(0);
            len += 1;
            continue;
        }
        // encoded sequence
        tmp.push_back(CODE);
        if (l >= 0x100) {
            if (l >= 0x300) {
                tmp.push_back(3);
                l -= 0x300;
                len -= 0x300;
            }
            tmp.push_back(l >> 8);
        }
        tmp.push_back(l);
        tmp.push_back(c);
        len += l;
    }
    result.swap(tmp);
}

void unRLE(std::vector<uint8_t>& v, std::vector<uint8_t>& result) {
    std::vector<uint8_t> tmp;
    for (int i = 0; i < v.size(); i++) {
        uint8_t c = v[i];
        if (CODE != c) {
            tmp.push_back(c);
            continue;
        }
        // sequence, use at() so it checks for out of bounds
        c = v.at(++i);
        if (0 == c) {
            tmp.push_back(CODE);
            continue;
        }
        size_t len = c;
        if (len < 4) {
            if (len == 3)
                len += v.at(++i);
            len = (len << 8) + v.at(++i);
        }
        c = v.at(++i);
        tmp.insert(tmp.end(), len, c);
    }
    result.swap(tmp);
}

size_t BMap::unpack(Bitstream& s) {
    for (auto& it : v) {
        uint8_t code;
        it = 0;
        s.pull(code, 2);
        switch (code) {
        case 0b11:
            it = ~it;
        case 0b00:
            continue;
        case 0b01:
            s.pull(it, 64);
            continue;
        }
        // code 10, secondary encoding, 4 quads
        for (int i = 0; i < 64; i += 16) {
            uint16_t q;
            s.pull(q, 2); // 0 is a NOP
            if (0b11 == q)
                q = 0xffff;
            else if (0b01 == q) // Secondary, as such
                s.pull(q, 16);
            else if (0b10 == q) { // Tertiary, need to read the code for this quart
                s.pull(code, 3); // three bits
                if (2 > code)
                    q = code ? 0xff00 : 0x00ff;
                else {
                    if (5 < code) { // Need one more code bit
                        s.pull(q, 1);
                        code = static_cast<uint8_t>(q | (code << 1));
                    }

                    // Need 7 bits
                    s.pull(q, 7);
                    switch (code) {
                    case 0b010: // 0b0010
                        break;
                    case 0b011: // 0b1000
                        q = q << 8;
                        break;
                    case 0b100: // 0b1101
                        q = q | 0xff80;
                        break;
                    case 0b101: // 0b0111
                        q = (q << 8) | 0x80ff;
                        break;
                    case 0b1100: // 0b0001 
                        q = (q | 0x80);
                        break;
                    case 0b1101: // 0b0100
                        q = (q | 0x80) << 8;
                        break;
                    case 0b1110: // 0b1110
                        q = q | 0xff00;
                        break;
                    default: // 0b1011, code is 0b1111
                        q = (q << 8) | 0xff;
                    }
                }
            }
            it |= static_cast<uint64_t>(q) << i;
        }
    }
    return v.size();
}

// 3-4 packing
size_t BMap::pack(Bitstream& s) {
    for (auto it : v) {
        if (0 == it or ~(0ULL) == it) {
            s.push(it & 0b11u, 2);
            continue;
        }

        // Test for switch to secondary
        uint8_t b;
        size_t halves = 0;
        for (size_t i = 0; i < 64; i += 8) {
            b = (it >> i) & 0xff;
            if (0 == b or 0xff == b)
                halves++;
        }

        if (halves < 2) { // Less than 2 halves, primary 64bit store
            s.push(0b01u, 2);
            s.push(it, 64);
            continue;
        }

        s.push(0b10u, 2); // switch to secondary, encoded by quart

        for (size_t j = 0; j < 4; j++, it >>= 16) {
            auto q = static_cast<uint16_t>(it);
            if (0 == q or 0xffff == q) {
                s.push(q & 0b11u, 2);
                continue;
            }

            // Test the two bytes, build the prefix code
            // 7 low bytes from the mixed byte get captured in val
            uint8_t code = 0;
            uint64_t val = 0;
            b = static_cast<uint8_t>(q >> 8); // High byte first
            if (0 == b or 0xff == b)
                code |= b & 0b11;
            else {
                val = b & 0x7f;
                code |= (val == b) ? 0b10: 0b01;
            }
            code <<= 2;
            b = static_cast<uint8_t>(q);
            if (0 == b or 0xff == b)
                code |= b & 0b11;
            else {
                val = b & 0x7f;
                code |= (val == b) ? 0b10: 0b01;
            }

            // Translate the prefix to tertiary codeword
            // Codes 6 to 15 are rotated to allow detection 
            // of code length from the lower 3 bits
            // b10 switch to tertiary encoding is added
            static uint8_t xlate[16] = {
                0xff,  // 0000 Not used
                0b011000 | 0b10,  // 0001 rotated from 0b1100
                 0b01000 | 0b10,  // 0010
                 0b00000 | 0b10,  // 0011
                0b111000 | 0b10,  // 0100 rotated from 0b0111
                0, // 0101 secondary 01, magic value
                0, // 0110 secondary 01, magic value
                0b10100 | 0b10,  // 0111
                0b01100 | 0b10,  // 1000
                0, // 1001 secondary 01, magic value
                0, // 1010 secondary 01, magic value
                0b111100 | 0b10,  // 1011 rotated from 0b1111
                 0b00100 | 0b10,  // 1100
                 0b10000 | 0b10,  // 1101
                0b011100 | 0b10,  // 1110 rotated from 0b1110
                0xff  // 1111 Not used
            };
            code = xlate[code];
            assert(code != 0xff);
            if (0 == code) { // Secondary, stored
                s.push((static_cast<uint64_t>(q) << 2) | 0b01u, 18);
                continue;
            }
            // Tertiary
            if (code < 0b111) { // No value
                s.push(code, 5);
                continue;
            }
            b = code < 0b011010 ? 5 : 6;
            s.push((val << b) | code, 7ull + b);
        }
    }

    return s.v.size();
}

// 2-3 packing
size_t BMap::altpack(Bitstream& s) {
    for (auto it : v) {
        if (0 == it or ~(0ULL) == it) {
            s.push(it & 0b11u, 2);
            continue;
        }

        // Test for switch to secondary
        uint8_t b;
        size_t halves = 0;
        for (size_t i = 0; i < 64; i += 8) {
            b = (it >> i) & 0xff;
            if (0 == b or 0xff == b)
                halves++;
        }

        if (halves < 2) { // Less than 2 halves, primary 64bit store
            s.push(0b01u, 2);
            s.push(it, 64);
            continue;
        }

        s.push(0b10u, 2); // switch to secondary, encoded by quart
        for (size_t j = 0; j < 4; j++, it >>= 16) {
            auto q = static_cast<uint16_t>(it);
            if (0 == q or 0xffff == q) {
                s.push(q & 0b11u, 2);
                continue;
            }

            // Test the two bytes, build the prefix code
            // The mixed half gets captured in val
            uint8_t code = 0;
            uint64_t val = 0;
            b = static_cast<uint8_t>(q >> 8); // High byte first
            if (0 == b or 0xff == b)
                code |= b & 0b11;
            else {
                val = b;
                code |= 0b01;
            }
            code <<= 2;
            b = static_cast<uint8_t>(q);
            if (0 == b or 0xff == b)
                code |= b & 0b11;
            else {
                val = b;
                code |= 0b01;
            }

            // Translate prefix, table includes the 0b10 switch to tertiary
            static uint8_t xlate[16] = {
                0xff,   // 00, Not used
                0b01000 | 0b10,  // 0x rotated from 0b100
                0xff,  // 0?, not used
                0b0000 | 0b10,  // 01
                0b11000 | 0b10,  // x0, rotated from 110
                0, // xx secondary 01, magic value
                0xff, // x?, not used
                0b11100 | 0b10,  // x1, rotated from 111
                0xff,  // ?0, not used
                0xff, // ?x, not used
                0xff, // ??, not used
                0xff,  // ?1, not used
                0b0100 | 0b10,  // 10
                0b01000 | 0b10,  // 1x, rotated from 101
                0xff,  // 1?, not used
                0xff    // 11, not used
            };
            code = xlate[code];
            assert(code != 0xff);
            if (0 == code) { // Secondary, stored
                s.push((static_cast<uint64_t>(q) << 2) | 0b01u, 18);
                continue;
            }
            if (7 > code) // Tertiary, no value
                s.push(code, 4);
            else // Tertiary
                s.push((val << 5) | code, 13);
        }
    }

    return s.v.size();
}
