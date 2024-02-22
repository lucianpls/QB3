# QB3: Fast and Efficient Image/Raster Compression

QB3 is an extremely fast raster specific lossless compression that compresses natural images
 considerably better then PNG. QB3 works on 2D rasters of integer values, signed and unsigned,
 from 8 to 64bit per value. Multiple color bands are supported. Compression and decompression 
 speed is 300MB/sec for byte data, much faster for higher byte count data types. QB3lib 
 has no external dependencies, has no significant memory footprint during operation, is single
 thread and very low complexity.

# Library
The library, located in [QB3lib](QB3lib) provides the core QB3 
algorithm implementation and a C language API.
It is implemented in C++11 and can be built for most platforms using 
cmake. It requires a little endian, two's complement architecture with 8, 16, 32 
and 64 bit integers, which includes the common AMD64 and ARM64 platforms.
Only 64bit builds should be used since the implementation uses 64 bit integers heavily.

# Using QB3
[cqb3](cqb3.md) is a utility conversion program that can convert PNG and JPEG images to and
from QB3, for 8 and 16 bit images. This utility does use an external library to read
and write JPEG and PNG images. The source code serves as an example of how to 
use the library.

Another option is to build [GDAL](https://github.com/OSGeo/GDAL) and
enable QB3 in MRF, which allows conversions to and from many other types of rasters.

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
C++ templates. The C API located in qb3encode.cpp and qb3decode.cpp 
adds a file format that in addition to the QB3 raw stream includes sufficient 
metadata to allow decoding.  
While the implementation is C++, it does not make use of any advanced features 
other than templates, conversion to C is very easy.

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
