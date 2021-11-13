# Generate C tables for QB3 encoding, for byte values

# It's not that easy in python due to infinite precision

# These only work for byte data, aka rung < 8
def mags(v):
    v &= 0xff
    return 0xff & ((0xff * (v >> 7)) ^ (v << 1))

def smag(v):
    return -((v >> 1) + 1) if (v & 1) else v >> 1

def show_magsign():
    for rung in range(2, 8):
        for v in range(-2**rung, 2**rung):
            print(f"{v:x}, {mags(v):x}, {smag(mags(v))}")

# These only work for short int data, aka rung > 7 and rung < 16
def magss(v):
    v &= 0xffff
    return 0xffff & ((0xffff * (v >> 15)) ^ (v << 1))

def smags(v):
    return -((v >> 1) + 1) if (v & 1) else v >> 1

def show_magsigns():
    for rung in range(8, 16):
        for v in range(-2**rung, 2**rung):
            print(f"{v}, {magss(v):x}, {smags(magss(v))}")

# Returns a 16 bit value, top 4 bits is payload size
# This system works up to rung 11
def encode(v, rung):
    if v < (1 << (rung - 1)): # Short
        return (rung << 12) + v + (1 << (rung - 1))
    elif v < (1 << rung): # Middle, rotated right one bit
        return ((rung + 1) << 12) + (v >> 1) + ((v & 1) << rung)
    else: # Long, rotated right two bits
        return ((rung + 2) << 12) + ((v - (1 << rung)) >> 2) + ((v & 3) << rung)

# Decode a var length unsigned int, returns value (lower rung - 1 bits) and source size bits 12+
def decode(v, rung):
    if v & (1 << (rung - 1)): # Short
        return (rung << 12) | (v & ((1 << (rung - 1)) - 1))
    if v & (1 << (rung - 2)): # Nominal
        return ((rung + 1) << 12) | ((v << 1) & ((1 << rung) - 1) | ((v >> rung) & 1))
    # long
    return ((rung + 2) << 12) | ((v << 2) & ((1 << (rung + 1)) - 1) | (1 << rung) | ((v >> rung) & 3))

# tables for byte data
def showencode8():
    for rung in range(2, 8):
        s = f"static const uint16_t crg{rung}[] = {{"
        for v in range(2**(rung+1)):
            s += f"0x{encode(v, rung):4x}, "
            if len(s) > 120:
                print(s)
                s = ""
        print(s[:-2] + "};")

# tables for rungs 8 to 10 (up to 11 bits, encoded fits in 12 bits)
def showencode16():
    for rung in range(8, 11):
        s = f"static const uint16_t crg{rung}[] = {{"
        for v in range(2**(rung+1)):
            s += f"0x{encode(v, rung):4x}, "
            if len(s) > 120:
                print(s)
                s = ""
        print(s[:-2] + "};")

def cs(v, u):
    'convert delta to codeword for codeswitch'
    sbitp = 1 << (u - 1) # Sign bit position
    if (v & sbitp): # Negative
        return mags(v - sbitp * 2)
    else: # Shift the positives, so zero becomes max pos
        return mags((v - 1) & (sbitp - 1))

def showcodeswitch():
    for ubits in (3,4,5,6):
        # If delta is zero, there is no code switch, just send the 0 bit
        s = f"static const uint16_t csw{ubits}[] = {{ 0x1000, "
        for v in range(1, 2**ubits):
            v = cs(v, ubits) # the magsign value
            v = encode(v, ubits - 1)
            # Add the change bit, change code and length
            v =  ((v + 0x1000) & 0xf000) | ((v << 1) & 0xff) | 1
            s += f"0x{v:04x}, "
            if len(s) > 120:
                print(s)
                s = ""
        print(s[:-2] + "};")

def showdecodeswitch():
    for ubits in (3, 4, 5, 6):
        s = f"static const uint16_t dsw{ubits}[] = {{ "
        for v in range(2**(ubits + 1)):
            v = decode(v, ubits - 1)
            # Account for the change bit
            sz = 1 + (v >> 12)
            v = smag(v & 0xff)
            if v >= 0:
                v = v + 1
            else:
                v = v & ((1 << ubits) -1)
            v |= sz << 12
            # All even values are no rung change, one bit
            s += f"0x{v:04x}, "
            if len(s) > 120:
                print(s)
                s = ""
        print(s[:-2] + "};")


# Check the code switch encode-decode
def trycs(ubits):
    enct = [0x1000,]
    for v in range(1, 2**ubits):
        v = cs(v, ubits) # the magsign value
        v = encode(v, ubits - 1)
        # Add the change bit, change code and length
        v =  ((v + 0x1000) & 0xf000) | ((v << 1) & 0xff) | 1
        enct.append(v)
    dect = []
    for v in range( 2**(ubits+1) ):
        v = decode(v, ubits -1)
        sz = 1 + (v >> 12)
        v = smag(v & 0xff)
        if v >= 0:
            v = v + 1
        else:
            v = v & ((1<< ubits) - 1) # Negative on ubits
        v |= sz << 12
        dect.append(v)
    # Try them
    for d in range(1 << ubits):
        print(f"{d} {enct[d]:04x} {dect[0xff & (enct[d] >> 1)]:04x}")
    for d in dect:
        print(f"{d:04x}")

if __name__ == "__main__":
    # showencode8()
    # showencode16()
    # showcodeswitch()
    showdecodeswitch()
#    trycs(6)