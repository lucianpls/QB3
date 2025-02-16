# QB3: Fast and Efficient Image/Raster Compression

- Lossless compression and decompression rate of 500MB/sec for byte data,
 close to 4GB/sec for 64 bit data
- Better compression than PNG in most cases
- No external dependencies, very low complexity
- No significant memory footprint during encoding or decoding
- All integer types, signed and unsigned, 8 to 64bit per value

# Library
The library, located in [QB3lib](QB3lib) provides a C API for the QB3 codec.
Implemented in C++, can be built on most platforms using cmake.
It requires a little endian, two's complement architecture with 8, 16, 32 
and 64 bit integers, which includes AMD64 and ARM64 platforms, as well as WASM.
Only 64bit builds should be used since this implementation uses 64 bit integers heavily.

# Using QB3
The included [cqb3](cqb3.md) command line image conversion program converts PNG 
or JPEG images to QB3, for 8 and 16 bit images. It can also decode QB3 to PNG.
The source code serves as an example of using the library.
This optional utility does depend on an external library to read and write 
JPEG and PNG images.

Another option is to build [GDAL](https://github.com/OSGeo/GDAL) with
QB3 in MRF enabled.

# C API
[QB3.h](QB3lib/QB3.h) contains the public C API interface.
The workflow is to create opaque encoder or decoder control structures, 
set or query options and encode or decode the raster data.
There are a couple of QB3 encoder modes. The default one is recommended. The other 
modes extend the encoding method, which results in slighlty better compression 
at the expense of encoding speed. For 8bit natural images the compression ratio 
gain from using the extended methods are usually very small, but can be considerable.
If even better compression ratio is needed, a good option is to combine the output
form the QB3 default mode with a second pass generic lossless compressions such 
as ZSTD or DEFLATE at a very low effort setting. This second pass is especially 
useful for synthetic images that include repeated identical sequences.  
Finally, a simplified mode that is 20% faster than the default and under 0.125% 
larger. The faster mode also speeds up decompression.

# Code Organization
The low level QB3 algorithm is implemented in the qb3decode.h and qb3encode.h as
C++ templates. While the core implementation is C++, it does not make use of 
any advanced features other than templates, conversion to C is very easy.
The higher level C API, located in qb3encode.cpp and qb3decode.cpp, which also 
adds a file interchange format.

# Change Log

## Version 1.3
- Bug fixes, performance improvements, more comprehensive testing
- Stride encoding, matching stride decoding
- QB3.h is the only public header

## Version 1.2.1
- Bug fix, decoding of best mode could fail for 32 and 64 bit integers, due to
overflow of an internal accumulator used to decode index groups
- Removed STL::sort encoding dependency, by including an implementation
of bubble sort. Since the sorted vector has at most 8 items, the use of STL
sort is overkill and increases size of the library without having a noticeable
performance benefit

## Version 1.2.0
- Speed optimizations, both compression and decompression
    - More than 400MB/sec for byte data using the default mode
- New QB3M_FTL mode
	- 500MB/sec for byte data, 25% faster than QB3M_DEFAULT
 	- Tiny compression penalty, under 0.1% in most cases
  	- Test availability by checking that QB3_HAS_FTL is defined
- WASM test
    - [prototype code](attic/world.cpp)

## Version 1.1.0
- Better scan ordering, second order Hilbert curve is the default
    - 5% better compression with no speed penalty
    - Original scan order (Morton) is optional
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

