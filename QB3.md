# QB3 technical details

## Introduction

QB3 is a raster specific lossless compression for integer values, signed and unsigned, up to 64bit per value. It achieves 
better compression than PNG for 8bit natural images while being extremely fast, for both compression and decompression. 

## Performance and Implementation

QB3 encodes and decodes 8 bit color imagery at a rate close to 300 MB per second using a single thread of a recent CPU. For 16, 32 and 64bit integer 
types it is much faster, reaching 2GB/s compression and 3GB/sec decompression. This means that only about 15 CPU clock cycles are needed per 
raw data value, measured on real cases, despite the relatively long dependency chains. At the same time, being a data type aware raster specific 
compression, QB3 achieves better compression than byte oriented compressors such as DEFLATE or ZSTD. The high rate is achieved by an implementation
that avoids code branches as much as possible. In addition, only basic arithmetic operations are used, with no multiplication or divisions. 
The performance does improve by about 10% using compiler auto-vectorization. Even higher performance could be achieved using manually tuned 
vectorization at the expense of portability. Parallel execution is also possible.  
Alternatively, better compression could be achieved using a more complex algorithm. A few extension algorithms are included, where
encoding speed drops by roughly half while decoding speed stays about the same. For 8 bit images the compression 
improvement is usually negligible. A better option is to use a generic lossless compression library such as ZSTD at a low effort level 
as a second encoding pass, the results are very good in both compression ratio and speed.

## QB3 Algorithm Overview

### Block Encoding

QB3 is based on encoding individual 4x4 micro blocks. The pixels within the microblock are scanned in a locality
preserving order. For version 1.0, the order is the legacy [Morton](https://en.wikipedia.org/wiki/Z-order_curve) order,
while version 1.1 uses the [Hilbert curve](https://en.wikipedia.org/wiki/Hilbert_curve), which generates a 
more efficient encoding.  
Within each band, blocks are aranged in row-major order. In case of multi-band images, band to band
decorrelation per pixel can be used. A band can be either a core band, in which case is left unmodified,
or a derived band, in which case pixel values from one of the core bands is subtracted from the raw values.
The values encoded are the differences between the current and the previous value, per band. The previous 
value starts as zero, and is maintained per band. The *previous value* is the previous value in the order of the 
bit interleaved scanning within the block, or the last value of the previous block within the same band. 
The goal of this algorithm is to produce groups with relatively small absolute values.
The next step is to convert the values within the block to magnitude-sign encoding. After the conversion,
the maximum value determines the number of bits per value required to hold the exact values for the whole group. 
This is the nominal *rung* of the block, the highest set bit index of the block maximum value. As long as the 
rung is known, the bits higher than the rung can be discarded as they are always zero. This is the main source 
of the QB3 compression.

### Magnitude-Sign encoding of integer values

For rasters, the locality preserving ordering is valuable when followed by delta encoding, which generate 
relatively small absolute values. The problem is that negative values require many bits in the normal 2s complement 
encoding. This is solved by reordering the values with alternate signs 0, -1, 1, -2 ..., up to the min_val. 
For encoding the values, this formula is: `m = abs(2 * v) - sign(v)`, where m has the same number of bits as the 
initial value v. This encoding stores the sign in bit 0, and the absolute value in the next higher bits, thus 
the top bits are zero. For negative values, the absolute value is biased by -1 because -0 is not a valid value, 
allowing the original range of values to be preserved, making the encoding reversible. This is the magnitude-sign 
(mags) encoding, because the absolute magnitude is followed by the sign bit. In this encoding the top zero bits 
are not needed to determine the actual value.  
In contrast, the usual 2s complement encoding is sign-magnitude (smag), and the magnitude of negative 
numbers is encoded with flipped bit values.

### MAGS QB3 suffix encoding

For this step, the input values resulting from the mag-sign encoding are considered as unsigned. For the 16 
values in a block, the highest bit set of any value is the rung of the block. Let's assume that for encoding 
values within a block N bits per value are required, which means that the block rung is n - 1. Within a block
 smaller values are more likely than larger ones, and re-encoding using a variable length code results in a 
 smaller output size. The block value range is split in three ranges, and values in each range is encoded 
 with a different number of bits.

 - First range, short, is the first quarter of possible values within a rung. These 
are the values between 0 and 2^(n-2), which start with 00 in the two most 
significant bits when stored on n bits.
 - Second range, nominal, is the second quarter of possible values, between 2^(n-2) and 2^(n-1). 
These values start with 01 in the two most significant bits.
 - Third range, long, are values between 2^(n-1) and 2^n. These represent the top 
 half of the possible valuesm, which have a 1 in the most significant bit. The maximum value per block is
always in this range, which means that at least one value per block is in this range.

The encoding type needs to be self-identifying, for every value. To do this, 
the encoded values end with one or two signal bits which determine the size of the symbol. Since the values
are stored in little endian format, these lower bits are stored and read first.
 - x0 Short
 - 01 Nominal
 - 11 Long

This means that values within each range get encoded with a different number of bits, including the suffix. 
Considering that the normal mags encoding for values in the n-1 rung requires n bits, in the prefix mags encoding we have:

 - Short:   1 + (n-2) = n-1 bits
 - Nominal: 2 + (n-2) = n bits
 - Long:    2 + (n-1) = n+1 bits
 
Values in the long range need one more bit than the nominal size, while values in the short range take one bit less.
If the number of values in the short range in a group is larger than the number of long values, the total number of bits 
for encoding the whole group will be smaller than the same group in mags encoding. Since this condition is very frequently 
true, the mags suffix encoding generally results in compression.

### Edge handling

QB3 is a block based encoding, yet the last block in a row or column may not be a full 4x4 block. The implementation avoids this 
limitation by shifting the begining of the last block in a row or column to the left or up so that the last block is always a 
full 4x4 block. This means that the last block duplicates values from the previous block when the full size is not a multiple 
of 4. While this results in sub-optimal compression, it is simple to implement and fast. The worst case is when both the width 
and the height of the image is of the form 4*N+1, where N is an integer. The maximum amount of redundant pixels expressed in 
percentage is thus `300 * (W + H) / (W*H)` or `600 / W` for a square image. While it may be significant for small images, 
this is a reasonable tradeoff for the simplicity of the implementation. For optimal results, the input should be a multiple of 
4 in both dimensions. If the image is padded, repeating the values of the nearest column and/or row is a good choice.

### QB3 Bitstream

The QB3 bitstream is a bit-dense concatenation of encoded groups, each encoded individually at a specific rung. In the case of multi 
band rasters, the group encodings for each band are stored in band interleaved order. 
For a group encoding, the rung of the group is encoded first, as the difference between the current rung and the 
previous rung for the same band. The rung values start with zero. The rung delta is encoded using QB3 code, at the rung for the maximum value of 
the rung for a data type. For byte data this will be rung 2, for 16bit integer 3. As a further optimization, assuming the group 
rung does not change often, a single 0 bit is used to signify that the rung value for the current block is identical to the one 
before. The rung change switch will be:

 - 0, If the rung is the same as the one before
 - 1 + CodeSwitch, If the rung is different

The CodeSwitch field encodes the delta between the current rung value and the previous one. This delta uses wrapparound at the 
number of bits required to hold all the possible bit values for a data type. Furthermore, since zero delta is encoded separately, 
the equivalent positive deltas are biased down by 1 (thus 0 means that delta is 1). This leaves one maximum positive value unused 
(for example for byte data, rung difference encoded as +4 would result in the same value as -4). This unused codeswitch is used 
as a SIGNAL for extended encoding.
The codeswitch itself is suffix encoded, using a variable number of bits. For example, for byte data, the codeswitch will use 2 bits for
+-1, 3 bits for +-2 and 4 bits for +-3, -4 and the SIGNAL.  
The codeswitch is followed by the 16 group values encoded in QB3 format described above. Special encoding are required at rung 0 and 
rung 1, where the normal QB3 encoding doesn't apply (output codes can be shorter than the two suffix bits).

### Rung 0 Encoding

At rung zero, each value requires a single bit, the QB3 ranges can't be used. There is however the special case when all the values 
within a block are zero. This happens when the input block is constant, which is common enogh to it deserves a special, shorter encoding.
For this case, the rung encoding is directly followed by a single zero bit. Using this encoding, blocks in large areas of constant 
value will use only two bits, both zero, the first one signifying that the block rung is the same as the one for the previous block 
(zero), while the next zero means that all the values within the block are zero. Otherwise, for a normal zero rung block where some of 
the values are ones, a 1 bit is stored (non-zero block), followed by the 16 bits for the 16 values. The special constant block encoding
determines the maximum achievable QB3 compression ratio of 64:1 for byte data (2 bits for a 16x8 micro-block). Without the explicit 
encoding of the constant block, the maximum compression ratio would have been much lower, under 8:1.

### Rung 1 Encoding
At rung 1, the low range is encoded using a single bit, following the QB3 encoding pattern.  
 - 0b00 -> 0b0
 - 0b01 -> 0b10
 - 0b10 -> 0b011
 - 0b11 -> 0b111

Decoding this rung has to be done slightly different from the higher rungs, testing each bit separately since the second bit might not exist.

### Group Step Encoding

The rung of a group is the highest bit set for any value within the group. This means that at least one of the values in 
the group is in the long range of the respective group. The encoder will not generate any encoded group where
all the values are in the nominal or short range. This kind of block would be a relatively small encoded group, but will always be
encoded at the next lowest rung.
Knowing this, it is possible to reduce the bit length of one of the values in an unambiguous way, so it can be restored when decoding. 
In the case where the first value within the group is the only one in the long range, the top bit can be flipped to zero, 
which changes the range of that value to the nominal or low. On decoding, if no value in the decoded group is in the long range, it can be
reasoned that the first value was the one reduced, and the original value can be restored by setting its rung bit. This eliminates 
group encodings where only the first value is in the long range. This encoding gap can then be used to reduce the second value in 
the group, if only the first two values are in the long range. This logic progresses all the way to reducing the last value in the group 
if all the values within the group are in the long range. This situation is also the worst encoding case, where the group would require 
16 more bits than the nominal. Using this reduction method, the worst case scenario requires at most 15 extra bits, since the last value
will be reduced.  
Another way to describe this is by looking at the sequence of the rung bits across the group. If the first n values have the
rung bit set while then rest of the values have it cleared, the last value with the bit set can be reduced on encoding and
restored on decoding. We know that at least one rung bit is set, so it is not possible to have all the rung bits clear 
initially. The sequnce of the rung bits is a step down function, going from 1 to 0, so this optimization is called 
*step encoding*. This optimization reduces the number of bits required to store a group by 1 or 2 bits when the sequence of the 
rung bit across the group is a step function. This step encoding applies to all rungs except at rung 0.

### Common Factor Group Encoding

This encoding is only used by the *best* mode of libQB3. It uses integer division and multiplication, operations which 
are not used in the default case. The non-zero values within a group may have a common factor *CF*. When CF is 2 or larger, 
the values within the group can be divided by CF before encoding, which results in at lease one rung reduction and thus shorter 
encoding. However, the CF value itself needs to be stored, and the CF encoding for the block has to be signaled to the decoder. 
In most cases the CF encoding is smaller than the normal QB3 one.

A CF encoded group is encoded as:
- SIGNAL. This is the unused value for a codeswitch, including 1 bit prefix.
- CodeSwitch for the group values, relative to the previous group rung. The prefix bit for this code switch is 1 if the same 
rung is used to encode CF value itself or 0 otherwise. The CodeSwitch is always present for CF encoding, the rung prefix bit 
has a different meaning when the CF SIGNAL was present. In this case, the CF rung could be the same as the previous rung value. 
Since the short codeswitch for same rung can't be used, the full SIGNAL encoding is used.
- If the bit immediately following the SIGNAL is zero, a second CodeSwitch, relative to the CF rung, encoding the rung for 
the CF itself.
- <CF - 2> value, encoded using the CF rung. It is biased down by 2 since the CF has to be at least 
2 for the CF group encoding. Note that the CF rung is the rung needed for the <CF-2> value, not CF.
- The reduced group values, using the CF group rung. This includes the group step encoding.

Notes:
- If CF group rung is zero and CF rung is also zero, the encoding uses
 the single codeswitch form (to rung 0), followed 
by the CF-2 bit, followed by the 16bits of the cf group. The short group 0 never
 occurs since at least one of the cf group is non-zero. The step-down optimization 
could be used here to reduce one sequence, but it would be extremely rare 
and would expand all other rung 0 sequences by one bit, likely having a negative effect overall.
- Since the CF group rung is reduced by division with CF, the CF group rung has the be at least one
 less than the maximum rung for the datatype. For byte data for example, the group rung can't be 7.  

TODO: use this to add index encoding, the code-switch flag bit is available.
- The separate CF rung encoding is used when CF-2 is in a larger rung than the 
rung for the CF group or when the CF-2 rung is much smaller than the group rung 
(this is an edge case, happens mostly for high byte count data). When CF is 
encoded with its own rung, CF is always in the top rung, so we can save one or 
more bits by enconding CF at the next lower rung.

## QB3 raster file format

The QB3 raster file adds a few metadata fields to the QB3 encoded stream, making it possible to decode
the image.  A QB3 image starts with a fixed QB3 header which has the following fields:

|Field|Description|Bytes|
|-|-|-|
|Signature| "QB3\200"|4|
|XSize| Width - 1|2|
|YSize| Height -1|2|
|Bands| Number of bands - 1|1|
|Type| Value type|1|
|Mode| Encoding mode|1|

- Multiple byte values are stored in little endian order.  
- The signature is used to identify the file as a QB3 file.
- The XSize and YSize fields are the width and height of the image, minus one. Images between 4x4 and 65536x65536 are supported.
    Bands is the number of bands in the image, minus one. Up to 256 bands are supported, although the library is normally compiled with a lower value.
- Type represents the value types. Currently integer types with 8, 16, 32 and 64 bits are supported. All values are reserved
- Mode represents the encoding style. Currently there are two modes, the default the *fast* mode. All values are reserved

The header is followed by a sequence of QB3 chunks. A QB3 chunk has a two character signature, followed by a two byte size field, 
followed by the chunk data. The chunk signature is used to identify the chunk type and the interpretation of the chunk data. The size is the 
size of the chunk data not including the signature and size fields. The following chunk types are currently defined, all other types are reserved.

|Signature|Name|Version|Description|Value|
|-|-|-|-|-|
|"CB"|Band mapping|1.0|A vector of core band number, per band|Number of bands|
|"QV"|Quanta Value|1.0|Multiplier for encoded values|A positive integer stored with the minimum number of bytes needed|
|"SC"|Scanning Curve|1.1|Scanning order of the microblock|A 64bit value that contains all 16 hex digit values, determining the order of pixels within a microblock|
|"DT"|Data|1.0|Pseudo chunk, directly followed by QB3 encoded stream|NA|

The "CB" is not present for a single band image or when the mapping is the identity.  
The "QV" chunk is not present when the quanta value is 1.  
The "SC" chunk is not written when the scanning curve is the legacy [Morton](https://en.wikipedia.org/wiki/Z-order_curve) order, 
to preserve compatibility with the 1.0 version of the format.
The "DT" chunk signature is used to signify the end of the chunks, and it is followed by QB3 encoded stream.  

Note that the "DT" chunk is the only chunk that does not have a size field. All the data immediately after the "DT" signature 
is part of the QB3 encoded bitstream. If the decoder is not provided with sufficient data to fully decode the image, 
it will return an error.

### Quantized image encoding

This lossy encoding step is used to improve compression further by storing the values in a pre-quantized form. The quantization is done by
dividing the input values by a constant, small integer quantization factor, rounding the results to the nearest integer value. 
The quantization factor is stored in a QB3 image header chunk. The quantized values are then encoded using the normal 
QB3 encoding. On decoding, the values decoded are multiplied by the quantization factor to restore the range
of the original values. Note that the range of the output values may be slightly different from the input values due to rounding.
While the QB3 stream encoding iteself is lossless, the quantization step is lossy. For some input images, the loss of some of the 
input information is not visually significant.  
