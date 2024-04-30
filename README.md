# QB3: Fast and Efficient Image/Raster Compression

- Better compression than PNG in most cases
- Compression and decompression speed is around 300MB/sec for RGB byte images, much 
faster for higher bit depth integer types
- Signed or unsigned integer values from 8 to 64bit per value
- No significant memory footprint
- Very low complexity
- The QB3 libray has no external dependencies

# Library
The library, located in [QB3lib](QB3lib) provides the core QB3 
algorithm implementation with a C API.
It is implemented in C++11 and can be built on most platforms using cmake.
It requires a little endian, two's complement architecture with 8, 16, 32 
and 64 bit integers, which includes the common AMD64 and ARM64 platforms.
Only 64bit builds should be used since this implementation uses 64 bit integers heavily.

# Using QB3
The included [cqb3](cqb3.md) utility conversion program converts PNG or JPEG images to QB3, 
for 8 and 16 bit images. It can also decode QB3 to PNG. The source code serves as an 
example of using the library.
This optional utility does have an external library dependency to read and write 
JPEG and PNG images.

Another option is to build [GDAL](https://github.com/OSGeo/GDAL) with
QB3 in MRF enabled, and then using gdal_translate to and from many other types of 
rasters.

# C API
[QB3.h](QB3lib/QB3.h) contains the public C API interface.
The workflow is to create opaque encoder or decoder control structures, 
set or query options and encode or decode the raster data. Finally, the control structures have to be destroyed.  
There are a couple of QB3 encoder modes. The default one is the fastest. The other 
modes extend the encoding method, which results in slighlty better compression 
at the expense of encoding speed. For 8bit natural images the compression ratio 
gain from using the extended methods are usually very small.
If the compression ratio gain warrants the extra complexity and dependecies, a better 
option is to combine the raster specific QB3 default output with a second pass generic 
lossless compressions such as ZSTD or DEFLATE, even at a very low setting. This is 
especially useful for synthetic images that include repeated identical sequences.

# Code Organization
The low level QB3 algorithm is implemented in the qb3decode.h and qb3encode.h as
C++ templates. While the core implementation is C++, it does not make use of 
any advanced features other than templates, conversion to C is very easy.
The higher level C API located in qb3encode.cpp and qb3decode.cpp, it
adds a file format that in addition to the QB3 raw stream includes sufficient 
metadata to allow decoding.

# Change Log
Version 1.0.0: Initial release
- C API
- All integer types

Version 1.1.0:
- Better scan ordering, second order Hilbert curve is the default
    - 5% better compression with no speed penalty
    - The legacy (Morton) scan order is available
- Small performance improvements and bug fixes
- Simplified code, removal of lookup tables for non-byte data
- Stride decoding, allowind decoding to non-contiguous line buffers
- Build system changes
    - Removed MSVC project, CMake is the only build system
    - Default build target is the library only, not the conversion program, 
    eliminating external dependencies
