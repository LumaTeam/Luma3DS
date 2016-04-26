#!/usr/bin/env python
# Requires Python >= 3.2 or >= 2.7

# This is part of Luma3DS

__author__    = "TuxSH"
__copyright__ = "Copyright (c) 2016 TuxSH" 
__license__   = "GPLv3"
__version__   = "v1.0"

import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Changes the path to Luma3DS for reboot patches")
    parser.add_argument("payload", help="Path to the Luma3DS payload")
    parser.add_argument("new_path", help="New Luma3DS payload path")
    args = parser.parse_args()
    data = b""

    if len(args.new_path) > 37:
        raise SystemExit("The new payload path is too large (37 characters max.)")

    with open(args.payload, "rb") as f: data = bytearray(f.read())

    if len(data) == 0: raise SystemExit("Could not read {0}".format(args.payload))

    if len(data) > 0x20000: 
        raise SystemExit("The input file is too large, are you sure you're using a Luma3DS payload?")

    found_index = data.find("sdmc:/".encode("utf-16-le"))

    if found_index == -1: 
        raise SystemExit("The pattern was not found, are you sure you're usinga a Luma3DS payload?")

    namebuf = args.new_path.encode("utf-16-le")
    namebuf += b'\x00' * (74 - len(namebuf))

    data[found_index + 12 : found_index + 12 + 74] = namebuf

    with open(args.payload, "wb+") as f: f.write(data)
