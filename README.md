# Experiments in 2D Raster Compression

This prototype has advanced to the point where it is ready to be incorporated in a raster format. It produces very good compression, 
comparable with PNG, while being extremely fast.

## Interbit addressing  
  b0 = Xb0  
  b1 = Yb0  
  b2 = Xb1  
  b3 = Yb1  
  b4 = Xb2  
  b5 = Yb2  
  ...  

    Xbk for k = 2i for i 0 to n/2  
    Ybk for k = 2i+1 for i 9 to n/2

An xy lookup table for 8x8 data points can be used to understand the spatial arangement

```
  0,  1,  4,  5, 16, 17, 20, 21,
  2,  3,  6,  7, 18, 19, 22, 23,
  8,  9, 12, 13, 24, 25, 28, 29,
 10, 11, 14, 15, 26, 27, 30, 31,
 32, 33, 36, 37, 48, 49, 52, 53,
 34, 35, 38, 39, 50, 51, 54, 55,
 40, 41, 44, 45, 56, 57, 60, 61,
 42, 43, 46, 47, 58, 59, 62, 63
```

Encoding is faster using a table, but only works for lookup tables of known size.  
For 8x8, the encoding is

    ibit = xy[y * 8 + x]

A recursive function works regardless of number of bits
```
int ibit(int x, int y) {
  if (0 == (x | y)) return 0;
  return (x & 1) | ((y & 1) << 1) | (ibit(x >> 1, y >> 1) << 2); 
}
```

The bit interleaved addressing can be extended to many dimensions.  
For 3D, every third bit is for Z.  A table for 4x4x4, 64 points, would be:
```
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
 54, 55, 62, 63


 ibit = xyz[z * 16 + y * 4 + x]
```

This indexing reduces the spatial distance, not the Hamming distance like Gray code. 
It is probably non-optimal, as an area filling line would be.  But it is simpler, 
and simpler to calculate

## Bitmap entropy encoding

When the datapoints are bits, the 8x8 2D group fits in a 64bit integer, which is available in most CPUs. The output is assumed to be a bit stream.  
Since the points are close, every byte represents a 4x2 group of pixels. They tend to be either all 0s or all 1s. 
This encoding uses this feature to store groups of 8x8 into a smaller number of bytes, if possible.

- Primary encoding
It is done at in 64 bit (8x8) groups. The 64bit value to be encoded is *val*. The prefix has two bits.  
  - 0b00 if *val* has all 0 bits
  - 0b11 if *val* has all 1 bits
  - 0b10 + secondary encoding mode, if at least two of the bytes are all 0s or all 1s
  - 0b01 + *val*, in little endian, otherwise

The secondary encoding is used when two or more of the 8 bytes are uniform. In this case the encoding is shorter than storing the full 64bit value, in the worse case it will take 62 bits.
- Secondary encoding of a 64bit group
Split the 64bit group in four 16bit quads, each representing a 4x4 area. Each quad is encoded individually, with a two bit prefix.  
  - 0b00 if *quad* has all 0 bits
  - 0b11 if *quad* has all 1 bits
  - 0b10 + tertiary encoding if either byte is uniform
  - 0b01 + *quad* , in little endian, otherwise

 - Tertiary encoding  
Separate the quad in two byes.  At least one of the bytes is all 0s or all 1s. All encodings are shorter than the 16 bits necessary otherwise.
We encode the state of each byte using two bits. 0b00 is a byte with all 0s, 0b11 is a byte with all 1s, 0b01 is any byte value > 127 and 0b10 is any byte value <= 127. Given that at least one of the two bytes is all 0s or all 1s, there are only 10 valid prefix combinations. These are further encoded into codewords using abbreviated binary.

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
Encoding containing a non-uniform byte (2-5 and 12-15) are followed by the lower 7 bits of the non-uniform byte, since the top bit is alreay known for the prefix.
The decoder needs sees the top three bits of the codeword before deciding that it needs a fourth, so the four bit codes are not stored as they are presented above. 
Instead, the top three bits are stored first, followed by the fourth. In other words, they are stored rotated right by one bit.

For example, a quad value of 0x5aff at the secondary encoding level will be encoded as:  

    0b10: tertiary encoding flag
The unpacked encoding would be 1110, with the 10 meaning high byte non-uniform with bit 7 == 0 and the 11 meaning low byte if ff
According to the table, the code word will be 1110. The top three bits are stored before the last bit, so it will be stored as:

    0b111  
    0b0  
    
The last part is the lower 7 bits of the non-uniform byte value, 0x5f in this case

    0b1011111

The result encoding size will use 2 +3 +1 +7 = 13 bits, instead of the 16 + 2 = 18 bits required to just store the value.
For 3 bit code words with a non-uniform byte, the encoding takea 12 bits, or only 5 bits if both halves of the quad are uniform.

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
The three bit codewords are stored rotated right one bit, to enble decoding. Those three bit codewords are followed by the 8 bits of the mixed byte.
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

When encoding large areas with all 0s or all 1s, the resulting bitstream may contains long sequences of 0s or 1s, even after the encoding described above achieves a 2:64 compression ratio. These repeated sequences can be further reduced by using a run length encoder (RLE) on the bitsream itself.

## Image encoding

For rasters, the locality preserving ordering is mostly valuable if we follow it up 
with delta encoding, it would generate relatively small values.  The problem with negative values is solved by 
reordering the values with alternate signs 0, -1, 1, -2 ..., up to the min_val.
For encoding the delta values, the formula becomes

    if (v < 0)
      m = -1 - 2 * v
    else
      m = 2 * v

where m has the same number of bits as the initial value v.  This encoding puts the sign in
bit 0, and the absolute value in the rest of the bits, thus the higher bits are zero 
for small values. For negative values, 1 is subtracted first, because -0 is not
needed, and it allows the initial range of values to be preserved.

In the compression stage, a local 8x8 or 4x4 group of values are stored in truncated binary encoding, 
as determined by the maximum m value in the respective group. To reconstruct the group values
from the truncated encoding, the maximum value in each group has to be transmitted before the group, allowing the 
receive to reconstruct each particular truncated encoding.


