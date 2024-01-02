# QB3: Fast and Efficient Image/Raster Compression

QB3 is a raster specific lossless compression that compresses better then PNG for natural images
while being more than one hundred times faster. QB3 works on 2D rasters of integer values, signed 
and unsigned, from 8 to 64bit per value. Multiple color bands are supported.

# Library
The library, located in [QB3lib](QB3lib) provides the core QB3 
algorithm implementation and a C language API.
It is implemented in C++11 and can be built for various platforms using 
cmake. It requires a little endian, two's complement architecture with 8, 16, 32 
and 64 bit integers, which includes the common AMD64 and ARM64 platforms.
Only 64bit builds should be used since the implementation heavily uses 64 bit integers.

# Using QB3
[cqb3](cqb3.md) is a utility conversion program that can convert PNG and JPEG images to and
from QB3, for 8 and 16 bit images. The source code serves as an example of how to 
use the library.

Another option is to build [GDAL](https://github.com/OSGeo/GDAL) and
enable QB3 in MRF.

# C API
[QB3.h](QB3lib/QB3.h) contains the public C API interface.
The workflow is to create opaque encoder or decoder control structures, 
then options and values can be set or querried and then the encode or 
decode are called. Finally, the control structures have to be destroyed.  
There are a few QB3 encoder modes. The default one is the fastest. The other 
encoder includes extended encoding methods which may result in better compression 
at the expense of encoding speed. For 8bit natural images the compression ratio 
gain is usually negligible. There is only one decoder.

# Code Organization
The low level QB3 algorithm is implemented in the qb3decode.h and qb3encode.h, as
C++ template functions. The C API located in qb3encode.cpp and qb3decode.cpp 
uses a file format that in addition to the QB3 raw stream includes sufficient 
metadata to allow decoding.

# Change Log
Version 1.0.0: Initial release

Version 1.1.0:
- New scan order, second order Hilbert curve is now the default
    - Better compression at the same or better speed
    - The old scan order is available
- Small performance improvements
- Bug fixes
- Stride decoding, allowind decoding to non-contiguous line buffers
- Build system improvements
    - Removed MSVC project, CMake is now the only build system
    - Default target is now the library, not the conversion program
    - cqb3 and the test program are not built by default