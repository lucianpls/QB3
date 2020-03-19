#pragma once

#include <cinttypes>
#include <vector>
#include <algorithm>

#if defined(_DEBUG)
#include <iostream>

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
	void getsize(int& x, int& y) {
		x = _x; y = _y;
	}
private:
	size_t unit(int x, int y) { return _lw * (y / 8) + x / 8; }
	int bitl(int x, int y) { return _xy[((y & 7) << 3) + (x & 7)]; }
	int _x, _y;
	size_t _lw; // Line width
	std::vector<uint64_t> _v;
};