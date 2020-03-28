#pragma once
#include <vector>
#include <cinttypes>

class Denc
{
public:
	Denc(int nbands = 3) : bands(nbands), prev(bands, 127) {};
	// Reads a vector of values, stored as a vector of deltas
	int delta(const std::vector<uint8_t>& input);
	// Restores the values, returns 0 if other is identical to v;
	int deltacheck(const std::vector<uint8_t>& other);
	// Changes the internal vector of deltas into a small value positive
	int recode();
	int bands;
	std::vector<uint8_t> prev;
	std::vector<uint8_t> v;
};

