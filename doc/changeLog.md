## Version 1.3.2
 - Significant performance improvements, especially for byte data on x86_64
 - PNG comparison updated to reflect the latest changes

## Version 1.3.1
- Made QB3M_FTL the default mode. The output is barely larger than QB3M_BASE 
 while being 25% faster for both encoding and decoding, a significant advantage
- Update QB3 algorithm description to match the current code
- Refactored RLE used in QB3M_BEST
- Add comparison with PNG document

## Version 1.3.0
- Stride encoding, matching stride decoding
- QB3.h is the only public header
- Performance improvements
- Improved testing

## Version 1.2.1
- Bug fix, decoding of best mode could fail for 32 and 64 bit integers, due to
overflow of an internal accumulator used to decode index groups
- Removed STL::sort encoding dependency, by including an implementation
of bubble sort. Since the sorted vector has at most 8 items, the use of STL
sort is overkill and increases size of the library without having a noticeable
performance benefit

## Version 1.2.0
- Speed optimizations, both compression and decompression
    - More than 400MB/sec for byte data using the QB3M_BASE mode
- New QB3M_FTL mode
	- 500MB/sec for byte data, 25% faster encoding than QB3M_BASE
    - Faster decoding than QB3M_BASE
 	- Tiny compression penalty, under 0.1% in most cases
  	- Test availability by checking that QB3_HAS_FTL is defined
- WASM test
    - [prototype code](attic/world.cpp)

## Version 1.1.0
- Hilbert second order space filling curve is the default scan order
    - 5% better compression with no speed penalty
    - Original scan order (Morton) made optional
- Minor performance improvements and bug fixes
- Simplified code, removal of lookup tables for non-byte data
- Allow stride decoding to non-contiguous line buffers
- Build system changes
    - Removed MSVC project
    - CMake is the only build system
    - Default build target is the library, eliminating external dependencies
    - Conversion utility is optional

## Version 1.0.0
- Initial release
- C API
- All integer types

