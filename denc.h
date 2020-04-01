#pragma once
#include <vector>
#include <cinttypes>

// block delta encode, sign and value, truncated binary
// Returns the size of the encoded result
std::vector<uint8_t> encode(
	std::vector<uint8_t> &image,
	size_t xsize, size_t ysize,
	size_t bsize = 4);

std::vector<uint8_t> siencode(
	std::vector<uint8_t>& image,
	size_t xsize, size_t ysize,
	size_t bsize = 4);
