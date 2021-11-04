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

# Returns a 16 bit value, top 4 bits is payload size
# This system would work up to rung 11
def encode(v, rung):
    if v < (1 << (rung - 1)): # Short
        return (rung << 12) + v + (1 << (rung - 1))
    elif v < (1 << rung): # Middle, rotated right one bit
        return ((rung + 1) << 12) + (v >> 1) + ((v & 1) << rung)
    else: # Long, rotated right two bits
        return ((rung + 2) << 12) + ((v - (1 << rung)) >> 2) + ((v & 3) << rung)

def showencode():
    for rung in range(2, 8):
        s = f"static const uint16_t crg{rung}[] = {{"
        for v in range(2**(rung+1)):
            s += f"0x{encode(v, rung):4x}, "
            if len(s) > 120:
                print(s)
                s = ""
        print(s[:-2] + "};")

if __name__ == "__main__":
    showencode()