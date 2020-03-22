# Encoding
## Interbit addressing  
  b0 = Xb0  
  b1 = Yb0  
  b2 = Xb1  
  b3 = Yb1  
  b4 = Xb2  
  b5 = Yb2  
  ...  
  bn :  
    Xbk for k = 2i for i 0 to n/2  
    Ybk for k = 2i+1 for i 9 to n/2

A lookup table for 8x8 = 64 data points can be used to understand the spatial arangement

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

Encoding is faster using than doing bit by bit, but only works for limited lookup tables of known size.  
For 8x8:  
ibit = xy[y * 8 + x]

A recursive works regardless of number of bits
```
int ibit(int x, int y) {
  if (0 == x && 0 == y)
    return 0;
  return (x & 1) | ((y & 1) << 1) | (ibit(x >> 1, y >> 1) << 2); 
}
```

Can be exetnded to many dimensions.  
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
```

This indexing minimizes the spatial distance, not the Hamming distance like Gray code

## Bitmap encoding

When the datapoints are bits, the 8x8 2D group fits in a 64bit integer, which is native in many CPUs. The output is assumed to be a bit stream.  
Since the points are close, every byte represents a 4x2 group of pixels. They tend to be either all 1 or all 0. 
These encoding use this feature to store groups of 8x8 into a smaller number of bytes.
- Primary encoding
Is done at the 64 bit level. The 64bit value encoding the 8x8 bits is *val*  
  - Encoding:  
0b00 if val is 0  
0b11 if val is ~0  
0b10 switches to secondary encoding mode, is the resulting size is smaller
0b01 followed by val, little endian otherwise  

- Secondary encoding, a 16bit quad, each representing a 4x4 area
  - quad encoding  
0b00 if whole quad is 0  
0b11 if whole quad is ~0  
0b10 + tertiary encoding if for two quarts either half of the quad is 0 or ~0, or if a quad is 0 ~0 or ~0 0
0b01 + quad value

 - Tertiary encoding, for a quad.  
Separate the quad in two byes.  At least one of the bytes is either 0 or ~0, otherwise the secondary encoding is used. 
There are 10 possible combinations, encoded in abbreviated binary.  If 0b00 signals a 0 byte, 0b11 is a ~0 byte, 0b01 is a byte > 127 and 0b10 is a byte <= 127, the combinations can written 
using 4 bytes and the abbreviated encodings are:

|Value |Encoded|
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
Encodings 0-3 and 12-15 are followed by the lower 7 bits of the non-uniform byte.