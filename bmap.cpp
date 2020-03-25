#include "bmap.h"
#include <iostream>
#include <algorithm>

BMap::BMap(int x, int y) : _x(x), _y(y), _lw((x + 7) / 8) {
	_v.assign(_lw * ((y + 7) / 8), ~0); // All data
}

int rlen(const uint8_t* ch, size_t mlen) {
	int r = 0;
	while (mlen-- && ch[0] == ch[r])
		r++;
	return r;
}

#define CODE 0xC5
void RLE(std::vector<uint8_t> &v, std::vector<uint8_t>& result) {
	// Then do an RLE across everything
	// This is buggy, needs a lot more tweaks

	std::vector<uint8_t> tmp;

	size_t len = 0;
	while (len < v.size()) {
		int l = rlen(&v[len], v.size() - len);
		uint8_t c = v[len];
		if (l < 4) {
			l = 1;
			if (CODE == c) {
				tmp.push_back(CODE);
				tmp.push_back(0);
			}
			else {
				tmp.push_back(c);
			}
		}
		else {
			tmp.push_back(CODE);
			if (l > 255)
				tmp.push_back(l >> 8);
			tmp.push_back(l);
			tmp.push_back(c);
		}
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
		// magic sequence, use at so it checks for overflow
		c = v.at(++i);
		if (0 == c) {
			tmp.push_back(CODE);
			continue;
		}
		size_t len = c;
		if (len < 4)
			len = len << 8 + v.at(++i);
		c = v.at(++i);
		tmp.insert(tmp.end(), len, c);
	}
	result.swap(tmp);
}

size_t BMap::pack(Bitstream& s) {
	for (auto it : _v) {
		// Primary encoding
		if (0 == it or ~0 == it) {
			s.push(it & 0b11, 2);
			continue;
		}

		// Test for secondary/tertiary
		// If at least two halves are 0 or 0xff
		int halves = 0;
		uint16_t q[4], b;
		for (int i = 0; i < 4; i++) {
			q[i] = static_cast<uint16_t>(it >> (i * 16));
			b = static_cast<uint8_t>(q[i]);
			if (0 == b or 0xff == b)
				halves++;
			b = static_cast<uint8_t>(q[i] >> 8);
			if (0 == b or 0xff == b)
				halves++;
		}

		if (halves < 2) { // Nope, encode as raw 64bit
			s.push(0b01, 2);
			s.push(it, 64);
			continue;
		}

		s.push(0b10, 2); // switch to secondary/tertiary, by quart
		for (int i = 0; i < 4; i++) {
			if (0 == q[i] or 0xffff == q[i]) { // short secondary
				s.push(q[i] & 0b11, 2);
				continue;
			}

			// This is a twice loop, can be written that way
			uint8_t code = 0, val = 0;
			b = q[i] & 0xff;
			if (0 == b or 0xff == b) {
				code |= b & 0b11;
			}
			else {
				val = b & 0x7f;
				code |= (val == b) ? 0b10 : 0b01;
			}

			code <<= 2;
			b = static_cast<uint8_t>(q[i] >> 8);
			if (0 == b or 0xff == b) {
				code |= b & 0b11;
			}
			else { // if val is overwritten here it is not needed
				val = b & 0x7f;
				code |= (val == b) ? 0b10 : 0b01;
			}

			// Translate the meaning to tertiary code
			static uint8_t xlate[16] = {
				0xf0,   // 0000 Not valid
				0b1100, // 0001
				0b010,  // 0010
				0b000,  // 0011
				0b1101, // 0100
				0xce,   // 0101 secondary 01, magic value
				0xce,   // 0110 secondary 01, magic value
				0b101,  // 0111
				0b011,  // 1000
				0xce,   // 1001 secondary 01, magic value
				0xce,   // 1010 secondary 01, magic value
				0b1111, // 1011
				0b001,  // 1100
				0b100,  // 1101
				0b1110, // 1110
				0xff    // 1111 Not valid
			};
			code = xlate[code];

			if (code < 2) { // Tertiary code, H split block
				s.push(code, 3);
				continue;
			}

			if (code < 0x10) { // Half uniform, tertiary
				s.push(code, (code < 6) ? 3 : 4);
				s.push(val, 7);
			} else { // magic value, secondary 16 bit raw
				s.push(0b01, 2);
				s.push(q[i], 16);
			}
		}
	}

	return s.v.size();
}

const uint8_t Bitstream::usemask[8] = {0, 0b1, 0b11, 0b111, 0b1111, 0b11111, 0b111111, 0b1111111};