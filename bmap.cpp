#include "bmap.h"
#include <iostream>
#include <algorithm>

const uint8_t Bitstream::mask[9] = { 0, 1, 3, 7, 15, 31, 63, 127, 255 };

BMap::BMap(int x, int y) : _x(x), _y(y), _lw((x + 7) / 8) {
	_v.assign(_lw * ((y + 7) / 8), ~0); // All data
}

int rlen(const uint8_t* ch, size_t mlen) {
	static const size_t MRUN = 0x300 + 0xffff;
	mlen = std::min(mlen, MRUN);
	const uint8_t *v = ch;
	while (mlen-- && *ch == *v) v++;
	return static_cast<int>(v - ch);
}

#define CODE 0xC5
void RLE(std::vector<uint8_t> &v, std::vector<uint8_t>& result) {
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
	for (auto &it : _v) {
		int code;
		it = 0;
		s.pull(code, 2);
		if (0b00 == code || 0b11 == code) {
			if (code)
				it = ~it;
			continue;
		}
		if (0b01 == code) { // Stored as such
			s.pull(it, 64);
			continue;
		}
		// code was 10, switch to secondary/tertiary
		for (int i = 0; i < 4; i++) {
			uint64_t q;
			s.pull(q, 2);
			// 0 is a NOP
			if (0b11 == q) {
				q = 0xffff;
			}
			else if (0b01 == q) { // Secondary, as such
				s.pull(q, 16);
			}
			else if (0b10 == q) { // Tertiary, need to read the code for this quart
				uint8_t code;
				s.pull(code, 3); // three bits, more frequent
				if (5 < code) { // code needs one more bit
					s.pull(q, 1);
					code = static_cast<uint8_t>((code << 1) | q);
				}

				if (2 > code) { // two halves, no value
					q = code ? 0xff00 : 0x00ff;
				}
				else {
					s.pull(q, 7); // Read the value and prepare the short int
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
					case 0b1111: // 0b1011
						q = (q << 8) | 0xff;  
						break;
					default: // error
						return 0;
					}
				}
			}
			it |= q << (i * 16);
		}
	}
	return _v.size();
}

size_t BMap::pack(Bitstream& s) {
	for (auto it : _v) {
		// Primary encoding
		if (0 == it or ~(0ULL) == it) {
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

		s.push(0b10, 2); // switch to secondary, by quart
		for (int i = 0; i < 4; i++) {
			if (0 == q[i] or 0xffff == q[i]) { // uniform secondary
				s.push(q[i] & 0b11, 2);
				continue;
			}

			// This is a twice loop, could be written that way
			uint8_t code = 0, val = 0;
			b = static_cast<uint8_t>(q[i] >> 8); // low byte first
			if (0 == b or 0xff == b) {
				code |= b & 0b11;
			}
			else {
				val = b & 0x7f;
				code |= (val == b) ? 0b10 : 0b01;
			}

			code <<= 2;
			b = static_cast<uint8_t>(q[i] & 0xff);
			if (0 == b or 0xff == b) {
				code |= b & 0b11;
			}
			else { // if val is overwritten here it is not needed
				val = b & 0x7f;
				code |= (val == b) ? 0b10 : 0b01;
			}

			// Translate the meaning to tertiary code
			static uint8_t xlate[16] = {
				0xff,   // 0000 Not valid
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

			if (code > 0xf) { // secondary 01, stored
				s.push(0b01, 2);
				s.push(q[i], 16);
				continue;
			}

			s.push(0b10, 2); // Tertiary encoding, one or two uniform halves
			// Push the top three bits of code first
			// This is because the decoder needs to see the top three
			// to decide if there is a fourth
			if (code < 6)
				s.push(code, 3);
			else // equiv to push(code>>1,3); push(code&1,1);
				s.push((code >> 1) | ((code & 1) << 3), 4);

			if (code > 1) // One half needs the 7 bits of data
				s.push(val, 7);
		}
	}

	return s.v.size();
}
