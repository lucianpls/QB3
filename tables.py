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

# Signaling is on the lowest 2 bits.
# x 0 -> bottom quarter
# 0 1 -> middle quarter
# 1 1 -> top half

# Returns a 16 bit value, top 4 bits is payload size
# This system works up to rung 11
def encode(v, rung):
    if rung == 0:
        return 0x1000 + v
    if v < (1 << (rung - 1)): # Short
        return (rung << 12) + v * 2
    elif v < (1 << rung): # Middle
        return ((rung + 1) << 12) + 4 * (v - (1 << (rung - 1))) + 1
    else: # Long, rotated right two bits
        return ((rung + 2) << 12) + 4 * (v - (1 << rung)) + 3

# Decode a var length unsigned int, returns value (lower rung - 1 bits) and source size bits 12+
def decode(v, rung):
    if rung == 0:
        return 0x1000 + (v & 1)
    if v & 1:
        if v & 2:
            v = v & ((1 << (rung + 2)) -1)
            return ((rung + 2) << 12) + (v >> 2) + (1 << rung)
        else:
            v = v & ((1 << (rung + 1)) -1)
            return ((rung + 1) << 12) + (v >> 2) + (1 << (rung - 1))
    else: # Short
        v = v & ((1 << rung) -1)
        return (rung << 12) + (v >> 1)


def print_table(t, s, formt : str):
    'Output a C formatted table'
    for v in t:
        s += formt.format(v);
        if len(s) > 120:
            print(s)
            s = ""
    if len(s) == 0:
        s = "  "
    print(s[:-2] + "};")

# tables for encoding, up to 11 bits
def showencode():
    for rung in range(11):
        out = []
        for v in range(2**(rung+1)):
            out.append(encode(v, rung))
        print_table(out, f"static const uint16_t crg{rung:x}[] = {{", "0x{:x}, ")

def showdecode():
    for rung in range(11):
        out = []
        for v in range(2**(rung + 2)):
            out.append(decode(v, rung))
        print_table(out, f"static const uint16_t drg{rung:x}[] = {{", "0x{:x}, ")

def trycodec():
    'Check encoding and decoding tables for all rungs'
    # Rung 0 is constant size
    assert 0x1000 == encode(0, 0) and 0x1000 == decode(0, 0)
    assert 0x1001 == encode(1, 0) and 0x1001 == decode(1, 0)

    for rung in range(1, 11):
        for v in range(2 ** (rung + 1)):
            size = encode(v, rung) >> 12
            assert size >= rung and size <= rung + 2, f"Size error at rung {rung}"
            code = encode(v, rung) & 0xfff
            assert (code >> size) == 0, "Encoding error at rung {rung}"

            if v < (1 << (rung - 1)): # Short, 4 encodings
                for x in range(4):
                    assert v == 0xfff & decode(code + (x << rung), rung),\
                        f"rung {rung}, {code + (x << rung):12b} decodes to {0xfff & decode(code + (x << rung), rung):12b}, expected {v:12b}"
            elif v < (1 << rung): # Middle, 2 encodings
                assert v == 0xfff & decode(code, rung),\
                    f"rung {rung}, {code:12b} decodes to {0xfff & decode(code, rung):12b}, expected {v:12b}"
                assert v == 0xfff & decode(code + (1 << (rung + 1)), rung),\
                    f"rung {rung}, {code + (1 << (rung + 1)):12b} decodes to {0xfff & decode(code + (1 << (rung + 1)), rung):12b}, expected {v:12b}"
            else: # Long, single encoding
                assert v == 0xfff & decode(code, rung),\
                    f"rung {rung}, {code:12b} decodes to {0xfff & decode(code, rung):12b}, expected {v:12b}"

def cs(v, u):
    'convert delta to codeword for codeswitch'
    sbitp = 1 << (u - 1) # Sign bit position
    if (v & sbitp): # Negative
        return mags(v - sbitp * 2)
    else: # Shift the positives, so zero becomes max pos
        return mags((v - 1) & (sbitp - 1))

def showcodeswitch():
    for ubits in (3, 4, 5, 6):
        # If delta is zero, there is no code switch, just send the 0 bit
        out = [0x1000]
        for v in range(1, 2**ubits):
            v = cs(v, ubits) # the magsign value
            v = encode(v, ubits - 1)
            # Add the change bit, change code and length
            v =  ((v + 0x1000) & 0xf000) | ((v << 1) & 0xff) | 1
            out.append(v)
        print_table(out, f"static const uint16_t csw{ubits}[] = {{", "0x{:x}, ")

# does not contain the change bit
# Need special encoding for middle value
def showdecodeswitch():
    for ubits in (3, 4, 5, 6):
        out = []
        for v in range(2**(ubits + 1)):
            v = decode(v, ubits - 1)
            # Account for the change bit
            sz = 1 + (v >> 12)
            v = smag(v & 0xff)
            if v >= 0:
                # Roll back to zero for max positive v, as a signal
                v = (v + 1) & ((1 << (ubits - 1)) - 1)
            else:
                v = v & ((1 << ubits) -1)
            out.append(v | (sz << 12))
        print_table(out, f"static const uint16_t dsw{ubits}[] = {{", "0x{:x}, ")

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
    for v in range(2**(ubits+1)):
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
    print(f"Ubits {ubits}, first and last column should be the same")
    for d in range(1, 1 << ubits):
        print(f"{d:04x} {enct[d]:04x} {dect[0xff & (enct[d] >> 1)]:x}")

# signal code-switch value by ubits
def showsame():
    out = [0, 0, 0]
    for ubits in range(3,7):
        v = cs(0, ubits)
        v = encode(v, ubits - 1)
        # Add the change bit, change code and length
        v =  ((v + 0x1000) & 0xf000) | ((v << 1) & 0xff) | 1
        out.append(v)
    print_table(out, "const uint16_t SIGNAL[] = {", "0x{:x}, ")

def show_double1():
    'Rung 1 double decoding table'
    single = tuple(decode(v, 1) for v in range(8))
    out = []
    for i in range(2 ** 6):
        v = single[i & 0x7] # First value
        v1 = single[(i >> (v >> 12)) & 0x7]; # Second value
        out.append((((v + v1) & 0xf000) >> 8) + (v1 & 0x3) * 4 + (v & 0x3))
    print_table(out, "static const uint8_t DDRG1[] = {", "0x{:x}, ")

def show_double2():
    'Rung 2 double decoding table'
    single = tuple(decode(v, 2) for v in range(16))
    out = []
    for i in range(2 ** 8):
        v = single[i & 0xf] # First value
        v1 = single[(i >> (v >> 12)) & 0xf]; # Second value
        out.append( ((v + v1) & 0xf000) + (v1 & 0x7) * 8 + (v & 0x7))
    print_table(out, "static const uint16_t DDRG2[] = {", "0x{:x}, ")

if __name__ == "__main__":
    #trycodec()
    #showencode()
    showdecode()

    #for i in 3,4,5,6:
    #    trycs(i)
    #showcodeswitch()
    #showdecodeswitch()

    #showsame()
    #show_double1()
    #show_double2()