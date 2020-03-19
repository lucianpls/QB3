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

int rlen(uint8_t* ch, size_t mlen) {
	int r = 0;
	while (mlen-- && ch[0] == ch[r]) { r++; }
	return r;
}

size_t BMap::pack(BitstreamOut &s) {
	for (auto it : _v) {
		if (0 == it || ~0 == it) {
			s.push(it & 0b11, 2);
		}
		else {
			bool quarts = false;
			uint16_t q[4];
			for (int i = 0; i < 4; i++) {
				q[i] = static_cast<uint16_t>(it >> (i * 16));
				if (0 == q[i] || 0xffff == q[i])
					quarts = true;
			}
			if (quarts) {
				s.push(0b10, 2);
				for (int i = 0; i < 4; i++)
					if (0 == q[i] || 0xffff == q[i]) {
						s.push(q[i] & 0b11, 2);
					}
					else {
						s.push(0b01, 2);
						s.push(q[i], 16);
					}
			}
			else {
				s.push(0b01, 2);
				s.push(it, 64);
			}
		}
	}

	// Then do an RLE across everything
	// This is buggy, needs a lot more tweaks

	std::vector<uint8_t> compr;

#define CODE 0xC5
	size_t len = 0;
	while (len < s.v.size()) {
		int l = rlen(&s.v[len], s.v.size() - len);
		if (l < 4) {
			l = 1;
			if (CODE == s.v[len]) {
				compr.push_back(CODE);
				compr.push_back(0);
			}
			else {
				compr.push_back(s.v[len]);
			}
		}
		else {
			// Missing the long sequence treatment, which is not common
//			std::cout << l << std::endl;
			compr.push_back(CODE);
			if (l > 255)
				compr.push_back(l >> 8);
			compr.push_back(l);
			compr.push_back(s.v[len]);
		}
		len += l;
	}
	return compr.size();
}