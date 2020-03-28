# Experiments in 2D Raster Compression


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

## Bitmap encoding

When the datapoints are bits, the 8x8 2D group fits in a 64bit integer, which is native in many CPUs. The output is assumed to be a bit stream.  
Since the points are close, every byte represents a 4x2 group of pixels. They tend to be either all 1 or all 0. 
These encoding use this feature to store groups of 8x8 into a smaller number of bytes.
- Primary encoding
Is done at in 64 bit (8x8) groups. The 64bit value is *val*
  - 0b00 if *val* has all 0 bits
  - 0b11 if *val* has all 1 bits
  - 0b10 switches to secondary encoding mode, if the resulting size is smaller
  - 0b01 followed by the full value, in little endian, otherwise

The secondary encoding is used when two or more of the 8 bytes are uniform. 
In that case the resulting encoding is shorter than storing the full value.

- Secondary encoding, a 16bit quad, each representing a 4x4 area
  - 0b00 if both bytes are 0
  - 0b11 if both bytes are 0xff
  - 0b10 + switch to tertiary encoding if either byte is uniform
  - 0b01 + quad value, in little endian, otherwise

 - Tertiary encoding, for a two byte quad.  
Separate the quad in two byes.  At least one of the bytes is all 0 or all 1. 
There are 10 possible combinations of two two bit codes, which are further encoded using abbreviated binary. 
If 0b00 signals a 0s byte, 0b11 is a 1s byte, 0b01 is any byte value > 127 and 0b10 is any byte value <= 127, the valid combinations can written 
using 4 bits and the corresponding abbreviated binary codewords are:

Value |Codeword|
|----|---:|
|0011| 000|
|1100| 001|
|0010| 010|
|1000| 011|
|1101| 100|
|0111| 101|
|0001|1100|
|0100|1101|
|1110|1110|
|1011|1111|

Combinations where more locations have the same value are shorter.
Encodings 2-5 and 12-15 are followed by the lower 7 bits of the non-uniform byte. The 8bit is part of the encoding itsels.
The decoder needs to see the top three bits first, so the four bit codes are not stored
as a 4 bit value. Instead, the top three bits are stored first, followed by the fourth.  

For example, a quad value of 0x5aff would be encoded as:  

    0b10: Switch to tertiary encoding  
The unpacked encoding would be 1110, with the 10 meaning high byte non-uniform with bit 7 == 0 and the 11 meaning low byte if ff
According to the table, the code word will be 1110. The top three bits 
are stored before the last bit, so it will be stored as:  

    0b111  
    0b0
The code word is followed by the lower 7 bits of the non-uniform byte value, 0x5f

    0b1011111

The result encoding size will use 2+3+1+7 = 13 bits, 
instead of the 16 + 2 = 18 bits required to just store the value. If only a 3 bit code word is needed the encoding would take 12 bits, or only 5 bits if both halves of the quad are uniform.
