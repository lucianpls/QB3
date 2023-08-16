# QB3: Fast and Efficient Image/Raster Compression

QB3 is a raster specific lossless compression that compresses better then PNG for natural images
while being more than one hundred times faster. QB3 works on rasters of integer values, signed 
and unsigned, from 8 to 64bit per value.

# QB3 Library
The library, located in [QB3lib](QB3lib) provides the core QB3 
algorithm implementation and provides a C language API.
It is implemented in standard C++11 and can be built for various platforms using 
cmake. It requires a little endian, two's complement architecture with 8, 16, 32 
and 64 bit integers, which includes the common AMD64 and ARM64 platforms.
Only 64bit builds should be used, since the implementation assumes efficient operations
on 64 bit values.

# Using QB3
[cqb3](cqb3.md) is a utility conversion program that can convert PNG and JPEG images to and
from QB3, for 8 and 16 bit images. The source code can also be used as an example of how to 
use the library in other applications.

Another option is to build [GDAL](https://github.com/OSGeo/GDAL) and
enable QB3 in MRF.

# C API
[QB3.h](QB3lib/QB3.h) contains the public C API interface.
The workflow is to create opaque encoder and decoder control structures, 
then options and values can be set and querried and then the encode or 
decode are called. Finally, the control structures have to be destroyed.  
There are two QB3 encoder modes. The default one is the fastest. The second 
encoder tries an additional way of encoding which may result in better compression 
but is slightly slower. For 8bit data, the difference in compression ratio is 
usually negligible. There is only one decoder.
The low level QB3 algorithm is implemented in the qb3decode.h and qb3encode.h, as
C++ template functions. The C API implements, located in qb3encode.cpp and qb3decode.cpp 
use a file format that in addition to the QB3 raw stream includes sufficient metadata to allow decoding.
