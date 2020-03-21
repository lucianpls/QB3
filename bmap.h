#pragma once

#include <cinttypes>
#include <vector>
#include <algorithm>
#include <cassert>

#if defined(_DEBUG)
#include <iostream>
#include <cstdio>

struct BStream {
	BStream() : len(0) {};
	size_t len;
	void push(uint64_t val, size_t bits) {
		for (int i = 0; i < bits; i++)
			std::cout << ((val >> i) & 1);
		std::cout << std::endl;
		len += bits;
	}
};
#endif

// XY addressing
extern uint8_t _xy[64];

// Output stream
struct BitstreamOut {
	std::vector<uint8_t> v;
	size_t len; // available bits in last byte
	BitstreamOut() : len(0) {};
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
};

struct BitstreamIn {
	std::vector<uint8_t> v;
	size_t pos; // Next bit position
	BitstreamIn(std::vector<uint8_t>& other) :
		v(other), pos(0) {};
	template<typename T = uint64_t> bool pull(T& val, size_t bits) {
		uint64_t acc = 0;
		while (bits && ((pos / 8) < v.size())) {
			size_t use = std::min(8 - pos % 8, bits);
			acc = (acc << use) | ((v[pos / 8] >> (pos % 8)) & usemask[use]);
			bits -= use;
			pos += use;
		}
		if (0 != bits)
			return false;
		val = static_cast<T>(acc);
		return true;
	}
private:
	static const uint8_t usemask[8];
};

class BMap {
public:
	BMap(int x, int y);
	virtual ~BMap() {};
	bool bit(int x, int y) {
		return 0 != (_v[unit(x, y)] & (1ULL << bitl(x, y)));
	}
	void set(int x, int y) {
		_v[unit(x, y)] |= 1ULL << bitl(x, y);
	}
	void clear(int x, int y) {
		_v[unit(x, y)] &= ~(1ULL << bitl(x, y));
	}
	size_t pack(BitstreamOut & stream);
	size_t dsize() { return _v.size()*sizeof(uint64_t); }
	void getsize(int& x, int& y) { x = _x; y = _y; }
	void dump(const std::string& name) {
		FILE* f = fopen(name.c_str(), "wb");
		fwrite(_v.data(), _v.size(), sizeof(uint64_t), f);
		fclose(f);
	}
private:
	size_t unit(int x, int y) {
		return _lw * (y / 8) + x / 8;
	}
	int bitl(int x, int y) {
		return _xy[((y & 7) << 3) + (x & 7)];
	}
	int _x, _y;
	size_t _lw; // Line width
	std::vector<uint64_t> _v;
};

void RLE(std::vector<uint8_t>& v, std::vector<uint8_t> &result);