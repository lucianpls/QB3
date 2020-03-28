#include "denc.h"

int Denc::delta(const std::vector<uint8_t> &input) {
	auto pval = input.data();
	for (auto& it : prev) it = 127; // Restore default 
	for (size_t i = 0; i < input.size() / bands; i++) {
		for (size_t c = 0; c < bands; c++) {
			v.push_back(*pval - prev[c]);
			prev[c] = *pval++;
		}
	}
	return 0;
}

int Denc::deltacheck(const std::vector<uint8_t>& input) {
	for (auto& it : prev) it = 127; // Restore defaults
	auto expected = input.data();
	auto d = v.data();
	for (size_t i = 0; i < input.size() / bands; i++) {
		for (size_t c = 0; c < bands; c++) {
			uint8_t val = *d++ + prev[c];
			if (val != *expected)
				return 1;
			prev[c] = *expected++;
		}
	}
	return 0;
}

int Denc::recode() {
	for (auto& it : v) {
		if (it & 0x80) {
			it = (3 - it) * 2 + 1;
		}
		else {
			it *= 2;
		}
	}
	return 0;
}