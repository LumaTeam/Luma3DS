#!/usr/bin/env python
# Requires Python >= 3.2 or >= 2.7

# This is part of AuReiNand

__author__    = "TuxSH"
__copyright__ = "Copyright (c) 2016 TuxSH" 
__license__   = "GPLv3"
__version__   = "v1.0"

"""
Parses AuReiNand exception dumps
"""

import argparse
from struct import unpack_from

# Source of hexdump: https://gist.github.com/ImmortalPC/c340564823f283fe530b
# Credits for hexdump go to the original authors
# Slightly edited by TuxSH

def hexdump(addr, src, length=16, sep='.' ):
    '''
    @brief Return {src} in hex dump.
    @param[in] length   {Int} Nb Bytes by row.
    @param[in] sep      {Char} For the text part, {sep} will be used for non ASCII char.
    @return {Str} The hexdump
    @note Full support for python2 and python3 !
    '''
    result = []

    # Python3 support
    try:
        xrange(0,1)
    except NameError:
        xrange = range

    for i in xrange(0, len(src), length):
        subSrc = src[i:i+length]
        hexa = ''
        isMiddle = False
        for h in xrange(0,len(subSrc)):
            if h == length/2:
                hexa += ' '
            h = subSrc[h]
            if not isinstance(h, int):
                h = ord(h)
            h = hex(h).replace('0x','')
            if len(h) == 1:
                h = '0'+h
            hexa += h+' '
        hexa = hexa.strip(' ')
        text = ''
        for c in subSrc:
            if not isinstance(c, int):
                c = ord(c)
            if 0x20 <= c < 0x7F:
                text += chr(c)
            else:
                text += sep
        result.append(('%08X:  %-'+str(length*(2+1)+1)+'s  |%s|') % (addr + i, hexa, text))

    return '\n'.join(result)
    

def makeRegisterLine(A, rA, B, rB):
    return "{0:<15}{1:<20}{2:<15}{3:<20}".format(A, "{0:08x}".format(rA), B, "{0:08x}".format(rB))
    
handledExceptionNames = ("FIQ", "undefined instruction", "prefetch abort", "data abort")
registerNames = tuple("r{0}".format(i) for i in range(13)) + ("sp", "lr", "pc", "cpsr")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Parse AuReiNand exception dumps")
    parser.add_argument("filename")
    args = parser.parse_args()
    data = b""
    with open(args.filename, "rb") as f: data = f.read()
    if unpack_from("<2I", data) != (0xdeadc0de, 0xdeadcafe):
        raise SystemExit("Invalid file format")
    
    processor, exceptionType, _, _, codeDumpSize, stackDumpSize = unpack_from("<6I", data, 12)
    
    print("Processor: ARM{0}".format(processor))
    print("Exception type: {0}".format("unknown" if exceptionType >= len(handledExceptionNames) else handledExceptionNames[exceptionType]))
    
    registers = unpack_from("<17I", data, 40)
    print("\nRegister dump:\n")
    for i in range(0, 16, 2): 
        print(makeRegisterLine(registerNames[i], registers[i], registerNames[i+1], registers[i+1]))
    print("{0:<15}{1:<20}".format(registerNames[-1], "{0:08x}".format(registers[-1])))
    
    codeDump = data[40+4*17 : 40+4*17 + codeDumpSize]
    print("\nCode dump:\n")
    print(hexdump(registers[15] - codeDumpSize + 2, codeDump))
    
    # Homebrew/CFW set their stack at 0x27000000, let's detect it
    if 0 <= 0x27000000 - registers[13] <= stackDumpSize: stackDumpSize = 0x27000000 - registers[13]
    
    stackOffset = 40+4*17 + codeDumpSize
    stackDump = data[stackOffset : stackOffset + stackDumpSize]
    print("\nStack dump:\n")
    print(hexdump(registers[13], stackDump))
    