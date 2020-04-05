#pragma once

#include <cinttypes>
#include <vector>
#include <algorithm>
#include <cassert>

#if defined(_DEBUG)
#include <iostream>
#include <cstdio>

//struct BStream {
//	BStream() : len(0) {};
//	size_t len;
//	void push(uint64_t val, size_t bits) {
//		for (int i = 0; i < bits; i++)
//			std::cout << ((val >> i) & 1);
//		std::cout << std::endl;
//		len += bits;
//	}
//};
#endif


// Output stream
struct Bitstream {
    std::vector<uint8_t>& v;
    size_t len; // available bits in last byte
    size_t pos; // Next bit for input
    Bitstream(std::vector<uint8_t>& data) : v(data), len(0), pos(0) {};
    void clear() {
        len = 0;
        pos = 0;
        v.clear();
    }

    void push(uint64_t val, size_t bits) {
        while (bits) {
            if (0 == len)
                v.push_back(static_cast<uint8_t>(val));
            else
                v.back() |= val << len;
            size_t used = std::min(8 - len, bits);
            bits -= used;
            len = (len + used) & 7;
            val >>= used;
        }
    }

    template<typename T = uint64_t> bool pull(T& val, size_t bits) {
        uint64_t acc = 0;
        int pulled = 0;
        while (bits && ((pos / 8) < v.size())) {
            size_t use = std::min(8 - pos % 8, bits);
            acc |= static_cast<uint64_t>((v[pos / 8] >> (pos % 8)) & mask[use]) << pulled;
            bits -= use;
            pos += use;
            pulled += static_cast<int>(use);
        }
        if (0 != bits)
            return false;
        val = static_cast<T>(acc);
        return true;
    }

    static const uint8_t mask[9];
};

class BMap {
public:
    BMap(int x, int y);
    ~BMap() {};
    bool bit(int x, int y) {
        return 0 != (_v[unit(x, y)] & (1ULL << bitl(x, y)));
    }
    void set(int x, int y) {
        _v[unit(x, y)] |= 1ULL << bitl(x, y);
    }
    void clear(int x, int y) {
        _v[unit(x, y)] &= ~(1ULL << bitl(x, y));
    }
    size_t dsize() { return _v.size() * sizeof(uint64_t); }
    void getsize(int& x, int& y) { x = _x; y = _y; }

    size_t pack(Bitstream& stream);
    size_t unpack(Bitstream& streamm);

    bool compare(BMap& other) {
        if (_v.size() != other._v.size())
            return false;
        for (int i = 0; i < _v.size(); i++) {
            if (_v[i] != other._v[i])
                return false;
        }
        return true;
    };

private:
    size_t unit(int x, int y) {
        return _lw * (y / 8) + x / 8;
    }
    int bitl(int x, int y) {
        // 2D interbit addressing
        static const uint8_t _xy[64] = {
            0,  1,  4,  5, 16, 17, 20, 21,
            2,  3,  6,  7, 18, 19, 22, 23,
            8,  9, 12, 13, 24, 25, 28, 29,
           10, 11, 14, 15, 26, 27, 30, 31,
           32, 33, 36, 37, 48, 49, 52, 53,
           34, 35, 38, 39, 50, 51, 54, 55,
           40, 41, 44, 45, 56, 57, 60, 61,
           42, 43, 46, 47, 58, 59, 62, 63
        };
        return _xy[((y & 7) << 3) + (x & 7)];
    }
    int _x, _y;
    size_t _lw; // Line width
    std::vector<uint64_t> _v;
    // XY addressing
    static const uint8_t _xy[64];
};

void RLE(std::vector<uint8_t>& v, std::vector<uint8_t>& result);
void unRLE(std::vector<uint8_t>& v, std::vector<uint8_t>& result);