#include "bmap.h"
#include <iostream>
#include <algorithm>

// XY order
uint8_t _xy[64] = {
	 0,  1,  4,  5, 16, 17, 20, 21,
	 2,  3,  6,  7, 18, 19, 22, 23,
	 8,  9, 12, 13, 24, 25, 28, 29,
	10, 11, 14, 15, 26, 27, 30, 31,
	32, 33, 36, 37, 48, 49, 52, 53,
	34, 35, 38, 39, 50, 51, 54, 55,
	40, 41, 44, 45, 56, 57, 60, 61,
	42, 43, 46, 47, 58, 59, 62, 63
};

BMap::BMap(int x, int y) : _x(x), _y(y), _lw((x + 7) / 8) {
	_v.assign(_lw * ((y + 7) / 8), ~0); // All data
}

int rlen(const uint8_t* ch, size_t mlen) {
	int r = 0;
	while (mlen-- && ch[0] == ch[r])
		r++;
	return r;
}

void RLE(std::vector<uint8_t> &v, std::vector<uint8_t>& result) {
	// Then do an RLE across everything
	// This is buggy, needs a lot more tweaks

	std::vector<uint8_t> compr;

#define CODE 0xC5
	size_t len = 0;
	while (len < v.size()) {
		int l = rlen(&v[len], v.size() - len);
		uint8_t c = v[len];
		if (l < 4) {
			l = 1;
			if (CODE == c) {
				compr.push_back(CODE);
				compr.push_back(0);
			}
			else {
				compr.push_back(c);
			}
		}
		else {
			compr.push_back(CODE);
			if (l > 255)
				compr.push_back(l >> 8);
			compr.push_back(l);
			compr.push_back(c);
		}
		len += l;
	}
	result.swap(compr);
}

size_t BMap::pack(BitstreamOut& s) {
	for (auto it : _v) {
		if (0 == it || ~0 == it) {
			s.push(it & 0b11, 2);
		}
		else {
			// If a single 16 bit quart is 0 or ~0, it's worth subdividing
			bool quarts = false;
			uint16_t q[4];
			for (int i = 0; i < 4; i++) {
				q[i] = static_cast<uint16_t>(it >> (i * 16));
				if (0 == q[i] || 0xffff == q[i])
					quarts = true;
			}
			if (quarts) {
				// subdivide the 64bit int in quarters
				s.push(0b10, 2);
				for (int i = 0; i < 4; i++)
					// Codes 00 and 11 means 0000 and ffff
					if (0 == q[i] || 0xffff == q[i]) {
						s.push(q[i] & 0b11, 2);
					}
					else {
						// We have two codes left, but subdiving doesn't make sense.
						// So we just use the codes to encode the top bit
						// 01 means bit 15 is 1, 10 means bit 15 is 0
						s.push((q[i] > 0x7fff) ? 0b01 : 0b10, 2);
						s.push(q[i] & 0x7fff, 15);
					}
			}
			else {
				s.push(0b01, 2);
				s.push(it, 64);
			}
		}
	}

	return s.v.size();
}

const uint8_t BitstreamIn::usemask[8] = {0, 0b1, 0b11, 0b111, 0b1111, 0b11111, 0b111111, 0b1111111};