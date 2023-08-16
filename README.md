# QB3: Fast and Efficient Image/Raster Compression

QB3 is a raster specific lossless compression that usually achieves better compression 
ratio than PNG for natural images while being more than one hundred times faster. It 
requires almost no memory during operation other than the input and output streams.
QB3 handles integer values, signed and unsigned, up to 64bit per value.

# QB3 Library
The library, located in [QB3lib](QB3lib) provides the core QB3 
algorithm implementation and provides a C language API.
It is implemented in standard C++11 and can be built for various platforms using 
cmake. It requires a little endian, two's complement architecture with 8, 16, 32 
and 64 bit integers, which includes the common AMD64 and ARM64 platforms. 
Only 64bit builds should be used, since the implementation makes heavy use of 64 
bit values.

# Use

[cqb3](cqb3.md) is a utility conversion program that can convert PNG and JPEG images to and
from QB3, for 8 and 16 bit images.  

Another option is to build [GDAL](https://github.com/OSGeo/GDAL) and
enable QB3 in MRF.  
[QB3.h](QB3lib/QB3.h) contains the public C API.
Opaque encoder and decoder control structures have to be created, then options and 
values can be set and querried and then the encode or decode functions can be 
called.
The data type size can be 8, 16, 32 or 64 bit integer, signed or unsigned.
There are two QB3 encoder modes. The default one is the fastest. The second 
encoder tries an additional way of encoding which may result in better compression 
but is slightly slower. For 8bit data, the difference in compression ratio is 
usually negligible. There is only one decoder.
