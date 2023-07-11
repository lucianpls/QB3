
# cqb3 Manual

cqb3 - convert an image file to and from QB3 format

Synopsis

cqb3 [ options ] filename [ output filename ]

Description

cqb3 reads the named input file and produces a QB3 file with the same name as the input filename and the **.QB3** extension. 
QB3 is a very efficient and very fast lossless image compression that supports 8, 16, 32 and 64 integer values.  
The cqb3 utility uses libicd for reading the input, which at the current time can read PNG and JFIF formatted images, with 8 and 16 bits per value. 
It can also decode a QB3 formatted input file and write it as a PNG file.

Options

Each option has to be preceeded by a - (dash) and separated by white spaces from any other option or argument.

-v
Verbose operation. Basic information about the input and output, compression ratios compared with raw input, as well as timing information 
is printed to standard error. Without this option only errors are printed.

-d
Decompress. Reads a QB3 formatted file and writes a PNG.

-b
Best. Turns on the **best** QB3 compression mode, which is slower but can produce better compression, especially for larger integer types.

-m <a,b,c,...>
band Mapping control. For images with more than one channel, QB3 can apply a band decorrelation filter which improves the compression. It does this
by subtracting one band from another. On decompression the effect of the filter is removed and the output image is identical to the input.
The default band mapping for color images with three or four bands assumes that the first three bands are the red, green and blue respectively and 
converts the input image to red - green, green, blue - green. The fourth band, if present, is left as is. The band mapping control allows this
default filter to be turned off, or the definition of a custom band mapping. Without a numerical argument, the band decorrelation filter is not 
applied (identity band mapping). The optional argument consist of a comma separated numerical list of band indexes which are to be subtracted from the
input bands. Band indexes are zero based. For the purpose of the band mapping, a band can be either a core band (unmodified) or derived (modified).
Only core bands can be subtracted from other bands. To mark a band as core, use it's band index as the argument in the band position in the band mapping.
Any other valid band index means that the respective core band will be subtracted from the respective band.
For example, the default RGB filter R-G,G,B-G is equivalent to the -m 1,1,1 argument, meaning that band 1 (green) is a core band (position 1 is 1), 
while band 1 (green) is to be subtracted from the 0 (red) and 2 (blue) bands. If the number of arguments is shorter than the number of bands in the
input, the unspecified band mappings are left unmodified (core). Following the same logic, the -m option with no parameters is equivalent to the
identity mapping, -m 0,1,2,... For RGBI (infrared) imagery, the 1,1,1,1 might be better than the default, which leaves the last band as is.
The QB3 compressor will adjust the band input mapping if the values are not valid, a warning will be printed by cqb3 when this happens.

-t
Trim. QB3 compression operates on 4x4 pixel blocks. When the input image size is not a multiple of 4x4, libQB3 will internally encode a few lines
and columns more than once. This may result in an output size that is slightly larger. When the trim option is present as a command line argument,
the input image will be trimmed to a multiple of 4x4 pixels before compression to QB3. The output QB3 raster size will reflect this trimmed size.
1, 2 or three lines and/or columns will be trimmed, in the last, then first, then last again order, as necessary to make the respective dimension 
a multiple of 4.
