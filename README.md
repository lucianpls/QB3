# Experiments in 2D Raster Compression

This prototype has advanced to the point where it is ready to be incorporated in a raster format. It produces very good compression, 
comparable with PNG, while being extremely fast.

## Bit Interleaved Index  
The goal is to use a single integral value as an index in a 2D array, by mixing bits from the X and Y index values  

  b0 = Xb0  
  b1 = Yb0  
  b2 = Xb1  
  b3 = Yb1  
  b4 = Xb2  
  b5 = Yb2  
  ...  

    Xbk for k = 2i for i 0 to n/2  
    Ybk for k = 2i+1 for i 9 to n/2

An xy lookup table for 8x8 points can be used to understand the spatial arangement. The table holds the single integral address value.

```
  xy = {
  0,  1,  4,  5, 16, 17, 20, 21,
  2,  3,  6,  7, 18, 19, 22, 23,
  8,  9, 12, 13, 24, 25, 28, 29,
 10, 11, 14, 15, 26, 27, 30, 31,
 32, 33, 36, 37, 48, 49, 52, 53,
 34, 35, 38, 39, 50, 51, 54, 55,
 40, 41, 44, 45, 56, 57, 60, 61,
 42, 43, 46, 47, 58, 59, 62, 63 }
```

Encoding is fast and easy using a lookup table, but it only works for lookup tables of known size. The table will also be very large if the input space is large. 
For an 8x8 input space, using the table above, the encoding is:

    ibit = xy[y * 8 + x]

A recursive function works regardless of number of bits, but it is slower than the lookup table. Every iteration adds two bits to the output index.
```
int ibit(int x, int y) {
  if (0 == (x | y)) return 0;
  return (x & 1) | ((y & 1) << 1) | (ibit(x >> 1, y >> 1) << 2); 
}
```

The bit interleaved addressing can be extended to many dimensions.  
For three dimensions, every third bit is for Z.  A lookup table for 4x4x4, 64 points, will be:
```
xyz = {
# Z = 0
  0,  1,  8,  9,
  2,  3, 10, 11,
 16, 17, 24, 25,
 18, 19, 26, 27,
# Z = 1
  4,  5, 12, 13,
  6,  7, 14, 15,
 20, 21, 28, 29,
 22, 23, 30, 31,
# Z = 2
 32, 33, 40, 41,
 34, 35, 42, 43,
 48, 49, 56, 57,
 50, 51, 58, 59,
# Z = 3
 36, 37, 44, 45,
 38, 39, 46, 47,
 52, 53, 60, 61,
 54, 55, 62, 63 }


 ibit = xyz[z * 16 + y * 4 + x]
```
A recursive index calculation function is also simple to write.  
This type of indexing reduces the spatial distance between successive points, not the Hamming distance like Gray code. 
It is probably suboptimal for this role, an area filling line would be better. But it is simpler to understand.

## Bitmap entropy encoding

When the datapoints are bits, an 8x8 group of values fits in a 64bit integer, a size which is available in most CPUs. The output is assumed to be a bit stream.  Every byte of the 64bit int represents a 4x2 group of pixels. Since they are close to each other, they tend to be either all 0s or all 1s.  
The encoding uses this feature to store groups of 8x8 into a smaller number of bytes, if possible. Each such group of 8x8 input bits is entropy encoded individually, using a three level prefix code.

- Primary encoding
It is done for each 64 bit (8x8) group. The 64bit value to be encoded is *val*. The encoding prefix has two bits.  
  - 0b00 if all bits are 0
  - 0b11 if all bits are 1
  - 0b10 + secondary encoding, if at least two of the 8 bytes contain all 0 or all 1 bits
  - 0b01 + *val*, in little endian, otherwise

- Secondary encoding of a 64bit group
The secondary encoding is used when two or more of the 8 bytes are uniform. In this case the encoding is shorter than storing the full 64bit value, in the worst case it will take 62 bits.
Split the 64bit group in four 16bit quads, each will represent a 4x4 area. Each quad is encoded, with a two bit encoding prefix.  
  - 0b00 if *quad* has all 0 bits
  - 0b11 if *quad* has all 1 bits
  - 0b10 + tertiary encoding if either byte is all 0 or all 1 bits
  - 0b01 + *quad* , in little endian, otherwise

 - Tertiary encoding  
Separate the quad in two byes. At least one of the bytes is all 0 or all 1 bits. All encodings are shorter than the 16 bits necessary otherwise.
We encode the state of each byte using two prefix bits. 0b00 is a byte with all 0s, 0b11 is a byte with all 1s, 0b01 is any byte value > 127 and 0b10 is any byte value <= 127. Given that one of the two bytes is all 0s or all 1s, there are 10 valid prefix combinations. These are further encoded into codewords using abbreviated binary.

|Prefix |Codeword|Rotated|
|----|---:|---:|
|0011| 000| 000|
|1100| 001| 001|
|0010| 010| 010|
|1000| 011| 011|
|1101| 100| 100|
|0111| 101| 101|
|0001|1100|0110|
|0100|1101|1110|
|1110|1110|0111|
|1011|1111|1111|

The codewords are chosen so that byte combinations where more bits have the same value are shorter.
Encoding containing a non-uniform byte (prefix values of 2-5 and 12-15) are followed by the lower 7 bits of the non-uniform byte, since the top bit is alreay known for the prefix.
The decoder needs sees the top three bits of the codeword before knowing if a fourth bit is needed, so the four bit codes are not stored as they are presented above. Since we use little endian encoding, the top three bits are stored first, followed by the fourth. In other words, they are stored in the bitstream rotated right by one bit, so the top three bits can be read first as a group.

For example, a quad value of 0x5aff at the secondary encoding level will be encoded as:  

    0b10: tertiary encoding flag
The unpacked encoding would be 1110, with the 10 meaning high byte non-uniform with bit 7 == 0 and the 11 meaning low byte if ff
According to the table, the code word will be 1110. The top three bits are stored before the last bit, so it will be stored as:

    0b111  
    0b0  
    
The last part is the lower 7 bits of the non-uniform byte value, 0x5f in this case

    0b1011111

The encoded value size will use 2 +3 +1 +7 = 13 bits, instead of the 16 + 2 = 18 bits required to just store the quad value.
For 3 bit code words with a non-uniform byte, the encoding takes 12 bits, or only 5 bits if both halves of the quad are uniform.

- Alternate tertiary encoding  
Could use a 2-3 abbreviated encoding:
In the prefix, if 0 is an all 0s byte, 1 is an all 1s byte and x represents a mixed byte, there are six possible combinations

|Prefix |Codeword|Rotated|
|----|---:|----:|
|01| 00| 00|
|10| 01| 01|
|0x|100|010|
|1x|101|110|
|x0|110|011|
|x1|111|111|

The 01 and 10 prefixes represent a 4x4 blocks where the top 4x2 region is all 0s and the bottom 4x2 region is all 1s, or the opposite.
The three bit codewords are stored rotated right one bit, to enable decoding. Those three bit codewords are followed by the 8 bits of the mixed byte.
The worst case size of the encoded quad is the same as the one in the initial encoding.

The same quad value of 0x5aff at the secondary encoding level will be encoded as:  

    0b10: tertiary encoding flag
The unpacked encoding would be x1,
According to the table, the code word will be 111. The top three bits are stored before the last bit, so it will be stored as:

    0b111
    
The last part is the mixed byte value, 0x5f in this case, all eight bits

    0b01011111

The result encoding size will use 2 +3 +8 = 13 bits, same as above. This size is constant, for all quads that contain one mixed byte.
If both bytes of the code are uniform, the encoding will take only 4 bits. This code is shorter, but there are no length 12 encodings. 
It might be easier to implement.

## Further entropy encoding

When encoding large areas with all 0s or all 1s, the resulting bitstream may contains long sequences of 0s or 1s, even after the encoding described 
above achieves the maximum 64:2 compression ratio. These repeated sequences can be further reduced by using a run length encoder (RLE) on the bitsream itself.

## Magnitude-Sign encoding of integer values

For rasters, the locality preserving ordering is mostly valuable if we follow it up 
with delta encoding, it would generate relatively small values. The problem is that negative values 
require many bits in the normal, two's complement encoding. This can be solved by 
reordering the values with alternate signs 0, -1, 1, -2 ..., up to the min_val.
For encoding the delta values, the formula becomes

    if (v < 0)
      m = -1 - 2 * v
    else
      m = 2 * v

where m has the same number of bits as the initial value v.  This encoding puts the sign in
bit 0, and the absolute value in the rest of the bits, thus the higher bits are zero 
for small values. For negative values, 1 is subtracted first, because -0 is not valid, which it allows the 
original range of values to be preserved. This is the magnitude-sign (mags) encoding, because the absolute 
magnitude is followed by the sign bit. The advantage is that in this encoding the top zero bits are not 
needed to determine the actual value.  
By contrast, the two's complement encoding is in sign-magnitude order (smag), although the magnitude of negative 
numbers is encoded with flipped bit values.

# Group Encoding

This is the basic encoding for raster data, in groups of 4x4, scanned in bit interleaved order. Within a band, blocks are in normal, 
row-major order, not in bit interleaved order. In case of multi-band images, one band may be designated as the main band. 
If a main band is selected, it is subtracted from all the other bands, pixel by pixel, before any other processing. The values 
to be encoded are the differences between the current and the previous value, per band. The previous value starts as zero, and is 
maintained per band. The *previous value* is the previous value in the order of the bit interleaved scanning within the block, 
or the last value of the previous block within the same band. The goal of this pre-processing is to produce groups with relatively 
small absolute values.
The next step is the conversion of the signed values within the block to mag-sign encoding. After the conversion, the maximum 
value will determine the number of bits per value required to hold the exact values for the whole group. This is the nominal 
*rung* of the block, the highest set bit index of the block maximum value. As long as the rung is known, the bits higher than 
the rung can be discarded as they contain no information. This is the main source of the QB3 compression.

# MAGS QB3 encoding

Let's assume that for encoding a block, the rung is n - 1, which means n bits per value are required. Since even within a 
block smaller values are more likely than larger ones, we split the possible values in three ranges, each encoded with a 
different number of bits.

 - First range, short, is the first quarter of possible codes. These values are between 0 and 2^(n-2), they start with two 
 zeros in the most significant bits when stored with n bits. They can be encoded using only n-2 bits.
 - Second range, the nominal values, are the second quarter of possible codes, between 2^(n-2) and 2^(n-1). These
values start with 01 in the two most significant bits. These also need n-2 bits.
 - Last type, long, are values between 2^(n-1) and 2^n. These represent the top half of the possible values, which occur 
less often. 
 - These values start with 1 in the most significant bit. These values will need n-1 bits for storage, we know that the top bit is 1.

The encoding type needs to be self-identifying, for every value. To do this, the range values are prefixed by:
 - 1x  Short
 - 01 Nominal 
 - 00 Long

This means that values within each range get encoded with a different number of bits, which include the prefix. 
Considering that the normal encoding requires n bits, we have:

 - Short: 1 + (n-2) = n-1 bits
 - Nominal: 2 + (n-2) = n bits
 - Long: 2 + (n-1) = n+1 bits
 
Values in the long range take one more bit than the nominal size, while values in the short range take one bit less. 
If the number of the short values in a group is larger than the number of long values, the overall size will be less 
than if the values in the group are stored with the nominal size. This variable size encoding results in additional 
compression.

# QB3 bitstream

The QB3 bitstream is a concatenation of encoded groups, each encoded individually at a specific rung. In the case of multi 
band rasters, the band groups are interleaved. The rung of each group is encoded first, as the difference between the current and the 
previous rung for the same band. The rung values start with the maximum rung possible for a given datatype. For example byte data will
have 7 as the starting rung. The rung delta is encoded using QB3 code, at the rung for the maximum value of the rung for a data type. 
For byte data this will be rung 2, for 16bit integer 3. As a further optimization, assuming the group rung does not change often, 
a single 0 bit is used to signify that the rung value for the current block is identical to the one before. The rung change switch will be:

 - 0 - If the rung is the same as the one before
 - 1 + CodeSwitch - If the rung is different

The CodeSwitch encodes the delta between the current rung and the previous one. This delta uses wrapparound at the 
number of bits required to hold all the possible bit values for a data type. Furthermore, since zero delta is encoded explicitly, 
the equivalent positive deltas are biased down by 1 (thus 0 means that delta is 1). This leaves one maximum positive value unused 
(for example for byte data, rung difference of +4 would result in the same value as -4). This special codeswitch is used as a SIGNAL.
The codeswitch itself is QB3 encoded, using a variable number of bits. For example, for byte data, the codeswitch will use 2 bits for
+-1, 3 bits for +-2 and 4 bits for +-3, -4 and the SIGNAL.

The codeswitch is then followed by the 16 group values, encoded in QB3 format. Special encoding are required at rung 0 and rung 1, where
the normal QB3 doesn't apply (output codes can be shorter than the two prefix bits).

## Rung 0 encoding

At rung zero, the values require a single bit. There is the special case when all the values within a block are zero. 
This happens relatively often, when the input block is constant, so it deserves a special, shorter encoding. In this encoding, 
the rung encoding is directly followed by a single zero. In this way, blocks in large areas of constant value will use only two bits, 
both zero, the first one signifying that the block rung is the same as the one for the previous block (zero), while the next zero
means that all the values within the block are zero.  
For a normal zero rung block, where some of the values are ones, a 1 bit is stored (non-zero block), followed by the 16 bits for the
16 values.

## Rung 1 encoding
At rung 1, the low range is encoded using a single bit, and as such it cannot follow the normal QB3 encoding pattern. 
Instead, the encoded values are:
 - 0 -> 0b0
 - 1 -> 0b11
 - 2 -> 0b001
 - 3 -> 0b101

These encoding follow the QB3 bit size pattern.

## Group Step encoding

The rung of a group is the highest bit set for any value within the group. This means that at least one of the values in 
the group is in the long range of the respective group. This means that the encoder will not produce an encoded group where
all the values are in the nominal or short range. This would be a relatively small encoded group, an opportunity wasted.
Knowing this, it is possible to reduce one of the values in an unambiguous way, so it can be restored when decoding. In the case
where the first value within the group is the only one in the long range, the top bit can be flipped to zero, which changes
the range of that value to the nominal or low. On decoding, if no values in the decoded group are in the long range, it is
assumed that the first value was reduced, and can be restored by setting the rung bit. This eliminates group encoding where
the first value is the only one in the long range. This can be used by reducing the second value in the group, if only the
first two values are in the long range. This progresses all the way to reducing the last value in the group, if all the values
within the group are in the long range. This situation is the worst case, where the group would require 16 extra bits compared
with the nominal. Using this reduction, the worst case scenario would require at most 15 extra bits, since the last value 
will be reduced.  
Another way to describe this is by looking at the squence of the rung bits across the group. If the first n values have the
rung bit set while then rest of the values have it cleared, the last value with the bit set can be reduced on encoding and
restored on decoding. We know that at least one rung bit is set, so it is not possible to have all the rung bits clear 
initially. The sequnce of the rung bits is a step down function, going from 1 to 0, so this optimization is called 
*step encoding*. if the rung-bit distribution is a step down, this optimization reduces the number of bits required to store 
a group by 1 or 2 bits. This encoding applies to any rung except for rung 0, since a single bit can't be reduced.

# Common Factor Group Encoding

In some cases, the non-zero values within a group have a common factor *CF*. If CF is 2 or larger, the values within the 
group can be divided by CF before encoding, which results in a rung reduction, thus a shorter encoding. However, the CF 
value itself needs to be stored, and the CF encoding has to be signaled to the decoder. A CF encoded group is encoded as:

- SIGNAL. This is the unused value for a codeswitch, including the 1 bit prefix.
- CodeSwitch for the CF group values, relative to the previous group rung. This encodes the rung used to store the 
reduced values in the CF group. The prefix bit is 1 if the same rung is used to encode CF value itself or 0 otherwise. 
The CodeSwitch is always present, the prefix bit has a different meaning when prefixed by the SIGNAL. In this case, the 
cf rung could be the same as the previous rung value. Since the short codeswitch can't be used, the SIGNAL encoding is 
used.
- If the bit immediately following the SIGNAL was zero, a second CodeSwitch, relative to the CF rung, encoding the rung for the CF itself.
- The CF - 2 value, encoded in QB3, using the CF rung. It is biased down by 2 since the CF has to be at least 
2 for the CF group encoding. Note that the CF rung is the rung for the CF-2 value, not CF.
- The 16 reduced group values, using the CF group rung. This includes the group step encoding.

Notes:
- If CF group rung is zero and CF rung is also zero, the encoding uses the single codeswitch form (to rung 0), followed 
by the CF-2 bit, followed by the 16bits of the cf group. The short group 0 never occurs since at least one of the cf 
group is non-zero. The step-down optimization could be used here to reduce one sequence, but it would be extremely rare 
and would expand all other rung 0 sequences by one bit, most likely having a negative effect overall.
- Since the CF group rung is reduced by division with CF, the CF group rung has the be at least one less than the maximum rung
for the datatype, since CF is at least 2. For byte data for example, CF group rung can't be 7.
- The separate CF rung encoding is used when CF-2 is in a larger rung than the rung for the CF group. CF - 2 is always in the top 
rung, so encoding the QB3 prefix bits is not needed...
