
# cqb3 Manual

cqb3 - convert an image file to and from QB3 format.

Synopsis

cqb3 [options] input-filename [output-filename]

Description

cqb3 reads the named input raster and produces a QB3 compressed file with the same name as the input and **.QB3** extension. 
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
Best. Turns on the **best** QB3 compression mode, which is slower than the default but can produce better compression, especially 
for larger integer types.

-f
Fast. Turns on the **fast** QB3 mode, which is faster than the default by about 10% while loosing less than .5 % of the compression. 
It is not compatible with -r.

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
A special case is "-m x", which tries all possible band mappings for RGB input and selects the one that gives the best compression. This option is
only valid for 3 or 4 band images. Note that if this option is provided, it will take about 10 times longer to finish the compression, since there
are 10 possible band combinations for RGB input.

-r
RLE. A run length encoding is applied after the QB3 compression. This can improve the compression ratio, especially for images with large areas of
constant values. The RLE encoding is also lossless, the original image is restored on decompression. The RLE encoding is not compatible with the fast mode.

-l
Legacy microblock scan order. Uses the Morton (Z) scan order for the 4x4 pixel blocks in the QB3 compression. This is the original, deprecated scan 
order for the microblock. The normal scan order is the Hilbert scan order, which results in better compression for most images. Use of this option is
not recommended.

-t
Trim. QB3 compression operates on 4x4 pixel blocks. When the input image size is not a multiple of 4x4, libQB3 will encode a few lines and columns twice.
When the trim option is present as a command line argument, the input image will be trimmed to a multiple of 4x4 pixels before compression to QB3. The 
output QB3 raster size will reflect this trimmed size. 1, 2 or three lines and/or columns may be trimmed, in the last, then first, then last again order, 
as necessary to make the respective dimension a multiple of 4.

-q <val>
Lossy encoding, by quantizing (dividing) input values by a small integer, before doing the actual QB3 decoding. On decoding, the decoded values 
are multiplied by the same value, restoring the normal range. The integer division is done with rounding, mid point to zero by default. Rounding 
can be changed to rounding mid point away from zero by adding a + sign before the value. If this option is not followed by a value, 2 is used. 
The divider must be within the range valid for the input data type, useful values are small integers.



