# QB3: Image/Raster Compression, Fast and Efficient 

- Better compression than PNG in most cases
- Lossless compression and decompression rate of 500MB/sec for byte data, 4GB/sec for 64 bit data
- All integer types, signed and unsigned, 8 to 64bit per value
- Lossless, or lossy by division with a small integer (quanta)
- No significant memory footprint during encoding or decoding
- No external dependencies, very low complexity

# Performance
Compared with PNG on a public image dataset, QB3 is 
[7% smaller while being 40 times faster](performance/performance.md)

![Compression vs PNG](performance/CID22_QB3vsPNG.svg)

# Library
The library, located in [QB3lib](QB3lib) provides a C API for the QB3 codec.
Implemented in C++, can be built on most platforms using cmake.
It requires a little endian, two's complement architecture with 8, 16, 32 
and 64 bit integers, which includes AMD64 and ARM64 platforms, as well as WASM.
Only 64bit builds should be used since this implementation uses 64 bit integers heavily.

# Using QB3
The included [cqb3](doc/cqb3.md) command line image conversion program converts PNG 
or JPEG images to QB3, for 8 and 16 bit images. It can also decode QB3 to PNG.
The source code serves as an example of using the library.
This optional utility does depend on an external library to read and write 
JPEG and PNG images.

Another option is to build [GDAL](https://github.com/OSGeo/GDAL) with
QB3 in MRF enabled.

[Web decoder demo](https://lucianpls.github.io/QB3/). This is a very simple leaflet 
based browser of a Landsat scene containing 8 bands of 16bit integer data. The QB3 tiles 
are decoded using the WASM decoder module every time the screen needs to be refreshed.
In QB3 format, this Landsat scene is half the size of the standard COG (TIFF) which uses 
LZW compression.

# C API
[QB3.h](QB3lib/QB3.h) contains the public C API interface.
The workflow is to create opaque encoder or decoder control structures, 
set or query options and encode or decode the raster data.  
There are a couple of QB3 encoder modes. The default mode (QB3M_FTL) is the fastest. 
The other modes extend the encoding method, which results in slighlty better 
compression at the expense of encoding speed. The QB3M_FTL mode is 25% faster 
than QB3M_BASE, while being less than .1% worse in compression ration. 
The QB3M_BEST is slower still, about half the speed of QB3M_BASE, but can 
sometimes result in significant compression ratio gains. For 8bit natural 
images the compression ratio gain from using the best mode are still small.  
If a better compression ratio is needed, a good option is to combine the output
form the QB3 fast mode with a second pass generic lossless compression such 
as ZSTD, at a very low effort setting (zip the QB3 output). This second pass 
is especially useful for synthetic images that contain identical sequences,
which QB3 does not compress well.

# Code Organization
The core lossless QB3 compression is implemented in qb3decode.h and qb3encode.h 
as C++ templates.  
The higher level C API is located in qb3encode.cpp and qb3decode.cpp,
which also define a file interchange format that contains the metadata needed 
for decoding. Lossy compression by quantization is also implemented in these files.

# Change Log

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

