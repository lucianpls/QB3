# QB3 technical details

**Incomplete**

# Group Encoding

This is the basic encoding for raster data, in groups of 4x4, scanned in bit interleaved order. 
Within a band, blocks are in normal, row-major order, not in bit interleaved order. 
In case of multi-band images, one band may be designated as the main band. If a 
main band is selected, it is subtracted from all the other bands, pixel by pixel, 
before any other processing. The values to be encoded are the differences between 
the current and the previous value, per band. The previous value starts as zero, and is 
maintained per band. The *previous value* is the previous value in the order of the bit interleaved scanning within the block, 
or the last value of the previous block within the same band. The goal of this pre-processing is to produce groups with relatively 
small absolute values.
The next step is the conversion of the signed values within the block to mag-sign encoding. After the conversion, the maximum 
value will determine the number of bits per value required to hold the exact values for the whole group. This is the nominal 
*rung* of the block, the highest set bit index of the block maximum value. As long as the rung is known, the bits higher than 
the rung can be discarded as they contain no information. This is the main source of the QB3 compression.

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

# MAGS QB3 prefix encoding

Let's assume that for encoding a block n bits per value are required, 
which means that the rung is n - 1. Even within a 16 value block smaller values 
are more likely than larger ones, using a variable length code will result in a 
smaller output size. We split the possible values in three ranges, each range 
encoded with a different number of bits.

 - First range, short, is the first quarter of possible codes within a rung. These 
are the values between 0 and 2^(n-2), which start with two zeros in the most 
significant bits when stored as n bits. These can be encoded using n - 1 bits, 
including a signal bit.
 - Second range, the nominal length values, are the second quarter of possible 
 codes, between 2^(n-2) and 2^(n-1). These values start with 01 in the two most 
significant bits. These values need n - 2 bits plus two signaling bits.
 - Last type, long, are values between 2^(n-1) and 2^n. These represent the top 
 half of the possible values, which occur less often. These values start with 1 
 in the most significant bit in normal encoding. These values will need n bits 
 for storage, including two signal bits, since we know that the top bit has to be 1.

The encoding type needs to be self-identifying, for every value. To do this, 
the range values are prefixed by one or two signal bits:
 - 1x Short
 - 01 Nominal 
 - 00 Long

This means that values within each range get encoded with a different number of bits, which include the prefix. 
Considering that the normal encoding requires n bits, we have:

 - Short:   1 + (n-2) = n-1 bits
 - Nominal: 2 + (n-2) = n bits
 - Long:    2 + (n-1) = n+1 bits
 
Values in the long range take one more bit than the nominal size, while values in the short range take one bit less. 
If the number of the short values in a group is larger than the number of long values, the overall size will be less 
than if the values in the group are stored with the nominal size. This variable size encoding results in additional 
compression.

# QB3 bitstream

The QB3 bitstream is a concatenation of encoded groups, each encoded individually at a specific rung. In the case of multi 
band rasters, the band for a given group are concatenated before going to the next group. 
The rung of each group is encoded first, as the difference between the current and the 
previous rung for the same band. The rung values start with the maximum rung possible for a given datatype. For example byte data will
have 7 as the starting rung. The rung delta is encoded using QB3 code, at the rung for the maximum value of the rung for a data type. 
For byte data this will be rung 2, for 16bit integer 3. As a further optimization, assuming the group rung does not change often, 
a single 0 bit is used to signify that the rung value for the current block is identical to the one before. The rung change switch will be:

 - 0, If the rung is the same as the one before
 - 1 + CodeSwitch, If the rung is different

The CodeSwitch encodes the delta between the current rung value and the previous one. This delta uses wrapparound at the 
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
At rung 1, the low range is encoded using a single bit, and as such it cannot follow the normal QB3 
encoding pattern.  
The used:
 - 0 -> 0b0
 - 1 -> 0b11
 - 2 -> 0b001
 - 3 -> 0b101

## Group Step encoding

The rung of a group is the highest bit set for any value within the group. This means that at least one of the values in 
the group is in the long range of the respective group. The encoder will not generate an encoded group where
all the values are in the nominal or short range. This would be a relatively small encoded group, an opportunity wasted.
Knowing this, it is possible to reduce one of the values in an unambiguous way, so it can be restored when decoding. In the case
where the first value within the group is the only one in the long range, the top bit can be flipped to zero, which changes
the range of that value to the nominal or low. On decoding, if no values in the decoded group are in the long range, it can be
reasoned that the first value was reduced, and can be restored by setting the rung bit. This eliminates group encoding where
the first value is the only one in the long range. This encoding can be used to reduce the second value in the group, if only the
first two values are in the long range. This progresses all the way to last value in the group if all the values
within the group are in the long range. This situation is the worst case, where the group would require 16 extra bits compared
with the nominal. Using this reduction, the worst case scenario would require at most 15 extra bits, since the last value 
will always be reduced.  
Another way to describe this is by looking at the squence of the rung bits across the group. If the first n values have the
rung bit set while then rest of the values have it cleared, the last value with the bit set can be reduced on encoding and
restored on decoding. We know that at least one rung bit is set, so it is not possible to have all the rung bits clear 
initially. The sequnce of the rung bits is a step down function, going from 1 to 0, so this optimization is called 
*step encoding*. if the rung-bit distribution is a step down, this optimization reduces the number of bits required to store 
a group by 1 or 2 bits. This encoding applies to any rung except for rung 0, since the rung of a single bit can't be reduced.

# Common Factor Group Encoding

This encoding is only used by the *best* mode of libQB3. It uses integer division and multiplication, operations which 
are not used in the default case.  
The non-zero values within a group may have a common factor *CF*. If CF is 2 or larger, the values within the 
group can be divided by CF before encoding, which results in a rung reduction and shorter encoding. However, 
the CF value itself needs to be stored, and the CF encoding has to be signaled to the decoder. Overall, there
are cases where the CF encoding is smaller than the normal QB3 one.  
A CF encoded group is encoded as:

- SIGNAL. This is the unused value for a codeswitch, including the 1 bit prefix.
- CodeSwitch for the CF group values, relative to the previous group rung. This encodes the rung used to store the 
reduced values in the CF group. The prefix bit is 1 if the same rung is used to encode CF value itself or 0 otherwise. 
The CodeSwitch is always present when using CF encoding, the rung prefix bit has a different meaning when the CF SIGNAL is present. 
In this case, the CF rung could be the same as the previous rung value. Since the short 
codeswitch for same rung can't be used, the full SIGNAL encoding is used.
- If the bit immediately following the SIGNAL is zero, a second CodeSwitch, relative to the CF rung, encoding the rung for the CF itself.
- CF - 2 value, encoded in QB3, using the CF rung. It is biased down by 2 since the CF has to be at least 
2 for the CF group encoding. Note that the CF rung is the rung needed for the CF-2 value, not CF.
- The 16 reduced group values, using the CF group rung. This includes the group step encoding.

Notes:
- If CF group rung is zero and CF rung is also zero, the encoding uses
 the single codeswitch form (to rung 0), followed 
by the CF-2 bit, followed by the 16bits of the cf group. The short group 0 never
 occurs since at least one of the cf group is non-zero. The step-down optimization 
could be used here to reduce one sequence, but it would be extremely rare 
and would expand all other rung 0 sequences by one bit, most likely having a negative effect overall.
- Since the CF group rung is reduced by division with CF, the CF group rung has the be at least one
 less than the maximum rung for the datatype, since CF is at least 2. For byte data for 
example, CF group rung can't be 7.  
TODO: use this to add index encoding, the code-switch flag bit is available.
- The separate CF rung encoding is used when CF-2 is in a larger rung than the 
rung for the CF group or when the CF-2 rung is much smaller than the group rung 
(this is an edge case, happens mostly for high byte count data). When CF is 
encoded with its own rung, CF is always in the top rung, so we can save one or 
more bits by enconding CF at the next lower rung.
