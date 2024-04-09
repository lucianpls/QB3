# QB3: Fast and Efficient Image/Raster Compression

QB3 compresses most images better than PNG while being extremely fast. QB3 works on 2D 
rasters of signed and unsigne integer values from 8 to 64bit per value. Both compression 
and decompression speed is around 300MB/sec for color byte images, while being much 
faster higher bit depth types. The QB3 libray has no external dependencies, no significant 
memory footprint during operation, and is very low complexity.

# Library
The library, located in [QB3lib](QB3lib) provides the core QB3 
algorithm implementation and a C language API.
It is implemented in C++11 and can be built for most platforms using 
cmake. It requires a little endian, two's complement architecture with 8, 16, 32 
and 64 bit integers, which includes the common AMD64 and ARM64 platforms.
Only 64bit builds should be used since the implementation uses 64 bit integers heavily.

# Using QB3
The included [cqb3](cqb3.md) utility conversion program converts PNG or JPEG images to QB3, 
for 8 and 16 bit images. It can also decode QB3 to PNG. The source code also serves as an 
example of how to use the library.
This utility does have an external library dependency to read and write JPEG and PNG images. 

Another option is to build [GDAL](https://github.com/OSGeo/GDAL) and
enable QB3 in MRF, and using gdal_translate conversions to and from many other types of 
rasters are available.

# C API
[QB3.h](QB3lib/QB3.h) contains the public C API interface.
The workflow is to create opaque encoder or decoder control structures, 
then options and values can be set or querried and then the encode or 
decode are called. Finally, the control structures have to be destroyed.  
There are a couple of QB3 encoder modes. The default one is the fastest. The other 
modes extend the encoding methods, which usually results in slighlty better compression 
at the expense of encoding speed. For 8bit natural images the compression ratio 
gain from using the extended methods are usually very small. There is only one decoder.
If the compression ratio warrants the extra complexity and dependecies, a better 
option is to combine the raster specific QB3 output with a second pass generic 
lossless compressions such as ZSTD or DEFLATE, even at a very low setting.

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
    - 5% better compression at the same speed
    - The legacy (Morton) scan order is available
- Small performance improvements and bug fixes
- Simplified code, removal of lookup tables for non-byte data
- Stride decoding, allowind decoding to non-contiguous line buffers
- Build system changes
    - Removed MSVC project, CMake is the only build system
    - Default build target is the library only, not the conversion program, 
    eliminating external dependencies
