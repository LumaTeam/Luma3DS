// SPDX-License-Identifier: MIT
// (c) 2024 LumaTeam

#pragma once
#include <3ds/types.h>

static const u8 ctrToSrgbTableTop[256][3] = {
    { 0x00, 0x00, 0x00, }, // i = 0x00
    { 0x01, 0x01, 0x01, }, // i = 0x01
    { 0x02, 0x01, 0x02, }, // i = 0x02
    { 0x03, 0x02, 0x03, }, // i = 0x03
    { 0x04, 0x03, 0x04, }, // i = 0x04
    { 0x05, 0x04, 0x05, }, // i = 0x05
    { 0x06, 0x04, 0x06, }, // i = 0x06
    { 0x07, 0x05, 0x06, }, // i = 0x07
    { 0x08, 0x06, 0x07, }, // i = 0x08
    { 0x08, 0x06, 0x08, }, // i = 0x09
    { 0x09, 0x07, 0x09, }, // i = 0x0A
    { 0x0A, 0x08, 0x0A, }, // i = 0x0B
    { 0x0B, 0x09, 0x0B, }, // i = 0x0C
    { 0x0C, 0x09, 0x0B, }, // i = 0x0D
    { 0x0D, 0x0A, 0x0C, }, // i = 0x0E
    { 0x0E, 0x0B, 0x0D, }, // i = 0x0F
    { 0x0F, 0x0B, 0x0E, }, // i = 0x10
    { 0x10, 0x0C, 0x0F, }, // i = 0x11
    { 0x10, 0x0D, 0x0F, }, // i = 0x12
    { 0x11, 0x0E, 0x10, }, // i = 0x13
    { 0x12, 0x0E, 0x11, }, // i = 0x14
    { 0x13, 0x0F, 0x12, }, // i = 0x15
    { 0x14, 0x10, 0x12, }, // i = 0x16
    { 0x15, 0x11, 0x13, }, // i = 0x17
    { 0x16, 0x11, 0x14, }, // i = 0x18
    { 0x16, 0x12, 0x15, }, // i = 0x19
    { 0x17, 0x13, 0x15, }, // i = 0x1A
    { 0x18, 0x14, 0x16, }, // i = 0x1B
    { 0x19, 0x14, 0x17, }, // i = 0x1C
    { 0x1A, 0x15, 0x18, }, // i = 0x1D
    { 0x1B, 0x16, 0x18, }, // i = 0x1E
    { 0x1C, 0x17, 0x19, }, // i = 0x1F
    { 0x1C, 0x17, 0x1A, }, // i = 0x20
    { 0x1D, 0x18, 0x1B, }, // i = 0x21
    { 0x1E, 0x19, 0x1B, }, // i = 0x22
    { 0x1F, 0x1A, 0x1C, }, // i = 0x23
    { 0x20, 0x1B, 0x1D, }, // i = 0x24
    { 0x21, 0x1B, 0x1E, }, // i = 0x25
    { 0x22, 0x1C, 0x1E, }, // i = 0x26
    { 0x23, 0x1D, 0x1F, }, // i = 0x27
    { 0x23, 0x1E, 0x20, }, // i = 0x28
    { 0x24, 0x1F, 0x21, }, // i = 0x29
    { 0x25, 0x1F, 0x21, }, // i = 0x2A
    { 0x26, 0x20, 0x22, }, // i = 0x2B
    { 0x27, 0x21, 0x23, }, // i = 0x2C
    { 0x28, 0x22, 0x24, }, // i = 0x2D
    { 0x29, 0x23, 0x24, }, // i = 0x2E
    { 0x2A, 0x24, 0x25, }, // i = 0x2F
    { 0x2B, 0x24, 0x26, }, // i = 0x30
    { 0x2C, 0x25, 0x27, }, // i = 0x31
    { 0x2D, 0x26, 0x28, }, // i = 0x32
    { 0x2E, 0x27, 0x28, }, // i = 0x33
    { 0x2E, 0x28, 0x29, }, // i = 0x34
    { 0x2F, 0x29, 0x2A, }, // i = 0x35
    { 0x30, 0x2A, 0x2B, }, // i = 0x36
    { 0x31, 0x2B, 0x2B, }, // i = 0x37
    { 0x32, 0x2B, 0x2C, }, // i = 0x38
    { 0x33, 0x2C, 0x2D, }, // i = 0x39
    { 0x34, 0x2D, 0x2E, }, // i = 0x3A
    { 0x35, 0x2E, 0x2F, }, // i = 0x3B
    { 0x36, 0x2F, 0x30, }, // i = 0x3C
    { 0x37, 0x30, 0x30, }, // i = 0x3D
    { 0x38, 0x31, 0x31, }, // i = 0x3E
    { 0x39, 0x32, 0x32, }, // i = 0x3F
    { 0x3A, 0x33, 0x33, }, // i = 0x40
    { 0x3B, 0x34, 0x34, }, // i = 0x41
    { 0x3C, 0x35, 0x34, }, // i = 0x42
    { 0x3D, 0x35, 0x35, }, // i = 0x43
    { 0x3E, 0x36, 0x36, }, // i = 0x44
    { 0x3F, 0x37, 0x37, }, // i = 0x45
    { 0x40, 0x38, 0x38, }, // i = 0x46
    { 0x41, 0x39, 0x39, }, // i = 0x47
    { 0x42, 0x3A, 0x3A, }, // i = 0x48
    { 0x43, 0x3B, 0x3A, }, // i = 0x49
    { 0x44, 0x3C, 0x3B, }, // i = 0x4A
    { 0x45, 0x3D, 0x3C, }, // i = 0x4B
    { 0x46, 0x3E, 0x3D, }, // i = 0x4C
    { 0x47, 0x3F, 0x3E, }, // i = 0x4D
    { 0x48, 0x40, 0x3F, }, // i = 0x4E
    { 0x49, 0x41, 0x40, }, // i = 0x4F
    { 0x4A, 0x42, 0x41, }, // i = 0x50
    { 0x4B, 0x43, 0x41, }, // i = 0x51
    { 0x4C, 0x44, 0x42, }, // i = 0x52
    { 0x4D, 0x45, 0x43, }, // i = 0x53
    { 0x4E, 0x46, 0x44, }, // i = 0x54
    { 0x4F, 0x47, 0x45, }, // i = 0x55
    { 0x50, 0x48, 0x46, }, // i = 0x56
    { 0x51, 0x49, 0x47, }, // i = 0x57
    { 0x52, 0x4A, 0x48, }, // i = 0x58
    { 0x53, 0x4B, 0x49, }, // i = 0x59
    { 0x54, 0x4C, 0x4A, }, // i = 0x5A
    { 0x55, 0x4D, 0x4B, }, // i = 0x5B
    { 0x56, 0x4E, 0x4B, }, // i = 0x5C
    { 0x57, 0x4F, 0x4C, }, // i = 0x5D
    { 0x58, 0x50, 0x4D, }, // i = 0x5E
    { 0x59, 0x51, 0x4E, }, // i = 0x5F
    { 0x5B, 0x52, 0x4F, }, // i = 0x60
    { 0x5C, 0x53, 0x50, }, // i = 0x61
    { 0x5D, 0x54, 0x51, }, // i = 0x62
    { 0x5E, 0x55, 0x52, }, // i = 0x63
    { 0x5F, 0x56, 0x53, }, // i = 0x64
    { 0x60, 0x57, 0x54, }, // i = 0x65
    { 0x61, 0x58, 0x55, }, // i = 0x66
    { 0x62, 0x59, 0x56, }, // i = 0x67
    { 0x63, 0x5A, 0x57, }, // i = 0x68
    { 0x64, 0x5B, 0x57, }, // i = 0x69
    { 0x65, 0x5C, 0x58, }, // i = 0x6A
    { 0x66, 0x5D, 0x59, }, // i = 0x6B
    { 0x67, 0x5E, 0x5A, }, // i = 0x6C
    { 0x68, 0x5F, 0x5B, }, // i = 0x6D
    { 0x69, 0x60, 0x5C, }, // i = 0x6E
    { 0x6A, 0x61, 0x5D, }, // i = 0x6F
    { 0x6B, 0x62, 0x5E, }, // i = 0x70
    { 0x6C, 0x63, 0x5F, }, // i = 0x71
    { 0x6D, 0x64, 0x60, }, // i = 0x72
    { 0x6E, 0x65, 0x61, }, // i = 0x73
    { 0x6F, 0x66, 0x62, }, // i = 0x74
    { 0x70, 0x67, 0x63, }, // i = 0x75
    { 0x71, 0x68, 0x64, }, // i = 0x76
    { 0x73, 0x69, 0x65, }, // i = 0x77
    { 0x74, 0x6A, 0x66, }, // i = 0x78
    { 0x75, 0x6B, 0x67, }, // i = 0x79
    { 0x76, 0x6C, 0x68, }, // i = 0x7A
    { 0x77, 0x6D, 0x68, }, // i = 0x7B
    { 0x78, 0x6E, 0x69, }, // i = 0x7C
    { 0x79, 0x6F, 0x6A, }, // i = 0x7D
    { 0x7A, 0x70, 0x6B, }, // i = 0x7E
    { 0x7B, 0x71, 0x6C, }, // i = 0x7F
    { 0x7C, 0x72, 0x6D, }, // i = 0x80
    { 0x7D, 0x73, 0x6E, }, // i = 0x81
    { 0x7E, 0x74, 0x6F, }, // i = 0x82
    { 0x7F, 0x75, 0x70, }, // i = 0x83
    { 0x80, 0x76, 0x71, }, // i = 0x84
    { 0x81, 0x77, 0x72, }, // i = 0x85
    { 0x82, 0x78, 0x73, }, // i = 0x86
    { 0x83, 0x79, 0x74, }, // i = 0x87
    { 0x84, 0x7A, 0x75, }, // i = 0x88
    { 0x85, 0x7B, 0x76, }, // i = 0x89
    { 0x86, 0x7C, 0x77, }, // i = 0x8A
    { 0x87, 0x7D, 0x78, }, // i = 0x8B
    { 0x88, 0x7E, 0x79, }, // i = 0x8C
    { 0x89, 0x7F, 0x7A, }, // i = 0x8D
    { 0x8A, 0x81, 0x7B, }, // i = 0x8E
    { 0x8B, 0x82, 0x7C, }, // i = 0x8F
    { 0x8C, 0x83, 0x7C, }, // i = 0x90
    { 0x8D, 0x84, 0x7D, }, // i = 0x91
    { 0x8E, 0x85, 0x7E, }, // i = 0x92
    { 0x8F, 0x86, 0x7F, }, // i = 0x93
    { 0x90, 0x87, 0x80, }, // i = 0x94
    { 0x91, 0x88, 0x81, }, // i = 0x95
    { 0x92, 0x89, 0x82, }, // i = 0x96
    { 0x93, 0x8A, 0x83, }, // i = 0x97
    { 0x94, 0x8B, 0x84, }, // i = 0x98
    { 0x95, 0x8C, 0x85, }, // i = 0x99
    { 0x96, 0x8D, 0x86, }, // i = 0x9A
    { 0x97, 0x8E, 0x87, }, // i = 0x9B
    { 0x98, 0x8F, 0x88, }, // i = 0x9C
    { 0x99, 0x90, 0x89, }, // i = 0x9D
    { 0x9A, 0x91, 0x8A, }, // i = 0x9E
    { 0x9B, 0x92, 0x8B, }, // i = 0x9F
    { 0x9C, 0x93, 0x8C, }, // i = 0xA0
    { 0x9D, 0x94, 0x8D, }, // i = 0xA1
    { 0x9E, 0x95, 0x8E, }, // i = 0xA2
    { 0x9F, 0x96, 0x8F, }, // i = 0xA3
    { 0xA0, 0x97, 0x90, }, // i = 0xA4
    { 0xA1, 0x98, 0x91, }, // i = 0xA5
    { 0xA2, 0x99, 0x92, }, // i = 0xA6
    { 0xA4, 0x9A, 0x93, }, // i = 0xA7
    { 0xA5, 0x9B, 0x94, }, // i = 0xA8
    { 0xA6, 0x9C, 0x95, }, // i = 0xA9
    { 0xA7, 0x9D, 0x96, }, // i = 0xAA
    { 0xA8, 0x9F, 0x97, }, // i = 0xAB
    { 0xA9, 0xA0, 0x98, }, // i = 0xAC
    { 0xAA, 0xA1, 0x99, }, // i = 0xAD
    { 0xAB, 0xA2, 0x9A, }, // i = 0xAE
    { 0xAC, 0xA3, 0x9B, }, // i = 0xAF
    { 0xAD, 0xA4, 0x9C, }, // i = 0xB0
    { 0xAE, 0xA5, 0x9D, }, // i = 0xB1
    { 0xAF, 0xA6, 0x9E, }, // i = 0xB2
    { 0xB0, 0xA7, 0x9F, }, // i = 0xB3
    { 0xB1, 0xA8, 0xA0, }, // i = 0xB4
    { 0xB2, 0xA9, 0xA1, }, // i = 0xB5
    { 0xB3, 0xAA, 0xA2, }, // i = 0xB6
    { 0xB4, 0xAB, 0xA3, }, // i = 0xB7
    { 0xB5, 0xAC, 0xA4, }, // i = 0xB8
    { 0xB6, 0xAD, 0xA5, }, // i = 0xB9
    { 0xB7, 0xAF, 0xA6, }, // i = 0xBA
    { 0xB8, 0xB0, 0xA7, }, // i = 0xBB
    { 0xB9, 0xB1, 0xA9, }, // i = 0xBC
    { 0xBA, 0xB2, 0xAA, }, // i = 0xBD
    { 0xBB, 0xB3, 0xAB, }, // i = 0xBE
    { 0xBC, 0xB4, 0xAC, }, // i = 0xBF
    { 0xBD, 0xB5, 0xAD, }, // i = 0xC0
    { 0xBE, 0xB6, 0xAE, }, // i = 0xC1
    { 0xBF, 0xB7, 0xAF, }, // i = 0xC2
    { 0xC0, 0xB9, 0xB0, }, // i = 0xC3
    { 0xC1, 0xBA, 0xB1, }, // i = 0xC4
    { 0xC3, 0xBB, 0xB2, }, // i = 0xC5
    { 0xC4, 0xBC, 0xB4, }, // i = 0xC6
    { 0xC5, 0xBD, 0xB5, }, // i = 0xC7
    { 0xC6, 0xBE, 0xB6, }, // i = 0xC8
    { 0xC7, 0xBF, 0xB7, }, // i = 0xC9
    { 0xC8, 0xC0, 0xB8, }, // i = 0xCA
    { 0xC9, 0xC2, 0xB9, }, // i = 0xCB
    { 0xCA, 0xC3, 0xBB, }, // i = 0xCC
    { 0xCB, 0xC4, 0xBC, }, // i = 0xCD
    { 0xCC, 0xC5, 0xBD, }, // i = 0xCE
    { 0xCD, 0xC6, 0xBE, }, // i = 0xCF
    { 0xCE, 0xC7, 0xBF, }, // i = 0xD0
    { 0xCF, 0xC8, 0xC1, }, // i = 0xD1
    { 0xD0, 0xCA, 0xC2, }, // i = 0xD2
    { 0xD2, 0xCB, 0xC3, }, // i = 0xD3
    { 0xD3, 0xCC, 0xC4, }, // i = 0xD4
    { 0xD4, 0xCD, 0xC5, }, // i = 0xD5
    { 0xD5, 0xCE, 0xC7, }, // i = 0xD6
    { 0xD6, 0xCF, 0xC8, }, // i = 0xD7
    { 0xD7, 0xD1, 0xC9, }, // i = 0xD8
    { 0xD8, 0xD2, 0xCB, }, // i = 0xD9
    { 0xD9, 0xD3, 0xCC, }, // i = 0xDA
    { 0xDA, 0xD4, 0xCD, }, // i = 0xDB
    { 0xDB, 0xD5, 0xCE, }, // i = 0xDC
    { 0xDC, 0xD7, 0xD0, }, // i = 0xDD
    { 0xDE, 0xD8, 0xD1, }, // i = 0xDE
    { 0xDF, 0xD9, 0xD2, }, // i = 0xDF
    { 0xE0, 0xDA, 0xD4, }, // i = 0xE0
    { 0xE1, 0xDB, 0xD5, }, // i = 0xE1
    { 0xE2, 0xDD, 0xD6, }, // i = 0xE2
    { 0xE3, 0xDE, 0xD8, }, // i = 0xE3
    { 0xE4, 0xDF, 0xD9, }, // i = 0xE4
    { 0xE5, 0xE0, 0xDA, }, // i = 0xE5
    { 0xE6, 0xE1, 0xDC, }, // i = 0xE6
    { 0xE7, 0xE3, 0xDD, }, // i = 0xE7
    { 0xE8, 0xE4, 0xDE, }, // i = 0xE8
    { 0xEA, 0xE5, 0xE0, }, // i = 0xE9
    { 0xEB, 0xE6, 0xE1, }, // i = 0xEA
    { 0xEC, 0xE7, 0xE2, }, // i = 0xEB
    { 0xED, 0xE9, 0xE4, }, // i = 0xEC
    { 0xEE, 0xEA, 0xE5, }, // i = 0xED
    { 0xEF, 0xEB, 0xE7, }, // i = 0xEE
    { 0xF0, 0xEC, 0xE8, }, // i = 0xEF
    { 0xF1, 0xED, 0xE9, }, // i = 0xF0
    { 0xF2, 0xEF, 0xEB, }, // i = 0xF1
    { 0xF3, 0xF0, 0xEC, }, // i = 0xF2
    { 0xF4, 0xF1, 0xEE, }, // i = 0xF3
    { 0xF5, 0xF2, 0xEF, }, // i = 0xF4
    { 0xF6, 0xF3, 0xF1, }, // i = 0xF5
    { 0xF7, 0xF5, 0xF2, }, // i = 0xF6
    { 0xF8, 0xF6, 0xF3, }, // i = 0xF7
    { 0xF9, 0xF7, 0xF5, }, // i = 0xF8
    { 0xFA, 0xF8, 0xF6, }, // i = 0xF9
    { 0xFB, 0xF9, 0xF8, }, // i = 0xFA
    { 0xFC, 0xFA, 0xF9, }, // i = 0xFB
    { 0xFD, 0xFC, 0xFA, }, // i = 0xFC
    { 0xFD, 0xFD, 0xFC, }, // i = 0xFD
    { 0xFE, 0xFE, 0xFD, }, // i = 0xFE
    { 0xFF, 0xFF, 0xFF, }, // i = 0xFF
};

// -------------------------------------------------

// For reference, the sRGB to CTR color space LUTs
// Above table(s) were computed with y = x - (f(x) - x)

static const u8 srgbToCtrTableTop[256][3] = {
    { 0x00, 0x00, 0x00, }, // i = 0x00
    { 0x01, 0x01, 0x01, }, // i = 0x01
    { 0x02, 0x03, 0x02, }, // i = 0x02
    { 0x03, 0x04, 0x03, }, // i = 0x03
    { 0x04, 0x05, 0x04, }, // i = 0x04
    { 0x05, 0x06, 0x05, }, // i = 0x05
    { 0x06, 0x08, 0x06, }, // i = 0x06
    { 0x07, 0x09, 0x08, }, // i = 0x07
    { 0x08, 0x0A, 0x09, }, // i = 0x08
    { 0x0A, 0x0C, 0x0A, }, // i = 0x09
    { 0x0B, 0x0D, 0x0B, }, // i = 0x0A
    { 0x0C, 0x0E, 0x0C, }, // i = 0x0B
    { 0x0D, 0x0F, 0x0D, }, // i = 0x0C
    { 0x0E, 0x11, 0x0F, }, // i = 0x0D
    { 0x0F, 0x12, 0x10, }, // i = 0x0E
    { 0x10, 0x13, 0x11, }, // i = 0x0F
    { 0x11, 0x15, 0x12, }, // i = 0x10
    { 0x12, 0x16, 0x13, }, // i = 0x11
    { 0x14, 0x17, 0x15, }, // i = 0x12
    { 0x15, 0x18, 0x16, }, // i = 0x13
    { 0x16, 0x1A, 0x17, }, // i = 0x14
    { 0x17, 0x1B, 0x18, }, // i = 0x15
    { 0x18, 0x1C, 0x1A, }, // i = 0x16
    { 0x19, 0x1D, 0x1B, }, // i = 0x17
    { 0x1A, 0x1F, 0x1C, }, // i = 0x18
    { 0x1C, 0x20, 0x1D, }, // i = 0x19
    { 0x1D, 0x21, 0x1F, }, // i = 0x1A
    { 0x1E, 0x22, 0x20, }, // i = 0x1B
    { 0x1F, 0x24, 0x21, }, // i = 0x1C
    { 0x20, 0x25, 0x22, }, // i = 0x1D
    { 0x21, 0x26, 0x24, }, // i = 0x1E
    { 0x22, 0x27, 0x25, }, // i = 0x1F
    { 0x24, 0x29, 0x26, }, // i = 0x20
    { 0x25, 0x2A, 0x27, }, // i = 0x21
    { 0x26, 0x2B, 0x29, }, // i = 0x22
    { 0x27, 0x2C, 0x2A, }, // i = 0x23
    { 0x28, 0x2D, 0x2B, }, // i = 0x24
    { 0x29, 0x2F, 0x2C, }, // i = 0x25
    { 0x2A, 0x30, 0x2E, }, // i = 0x26
    { 0x2B, 0x31, 0x2F, }, // i = 0x27
    { 0x2D, 0x32, 0x30, }, // i = 0x28
    { 0x2E, 0x33, 0x31, }, // i = 0x29
    { 0x2F, 0x35, 0x33, }, // i = 0x2A
    { 0x30, 0x36, 0x34, }, // i = 0x2B
    { 0x31, 0x37, 0x35, }, // i = 0x2C
    { 0x32, 0x38, 0x36, }, // i = 0x2D
    { 0x33, 0x39, 0x38, }, // i = 0x2E
    { 0x34, 0x3A, 0x39, }, // i = 0x2F
    { 0x35, 0x3C, 0x3A, }, // i = 0x30
    { 0x36, 0x3D, 0x3B, }, // i = 0x31
    { 0x37, 0x3E, 0x3C, }, // i = 0x32
    { 0x38, 0x3F, 0x3E, }, // i = 0x33
    { 0x3A, 0x40, 0x3F, }, // i = 0x34
    { 0x3B, 0x41, 0x40, }, // i = 0x35
    { 0x3C, 0x42, 0x41, }, // i = 0x36
    { 0x3D, 0x43, 0x43, }, // i = 0x37
    { 0x3E, 0x45, 0x44, }, // i = 0x38
    { 0x3F, 0x46, 0x45, }, // i = 0x39
    { 0x40, 0x47, 0x46, }, // i = 0x3A
    { 0x41, 0x48, 0x47, }, // i = 0x3B
    { 0x42, 0x49, 0x48, }, // i = 0x3C
    { 0x43, 0x4A, 0x4A, }, // i = 0x3D
    { 0x44, 0x4B, 0x4B, }, // i = 0x3E
    { 0x45, 0x4C, 0x4C, }, // i = 0x3F
    { 0x46, 0x4D, 0x4D, }, // i = 0x40
    { 0x47, 0x4E, 0x4E, }, // i = 0x41
    { 0x48, 0x4F, 0x50, }, // i = 0x42
    { 0x49, 0x51, 0x51, }, // i = 0x43
    { 0x4A, 0x52, 0x52, }, // i = 0x44
    { 0x4B, 0x53, 0x53, }, // i = 0x45
    { 0x4C, 0x54, 0x54, }, // i = 0x46
    { 0x4D, 0x55, 0x55, }, // i = 0x47
    { 0x4E, 0x56, 0x56, }, // i = 0x48
    { 0x4F, 0x57, 0x58, }, // i = 0x49
    { 0x50, 0x58, 0x59, }, // i = 0x4A
    { 0x51, 0x59, 0x5A, }, // i = 0x4B
    { 0x52, 0x5A, 0x5B, }, // i = 0x4C
    { 0x53, 0x5B, 0x5C, }, // i = 0x4D
    { 0x54, 0x5C, 0x5D, }, // i = 0x4E
    { 0x55, 0x5D, 0x5E, }, // i = 0x4F
    { 0x56, 0x5E, 0x5F, }, // i = 0x50
    { 0x57, 0x5F, 0x61, }, // i = 0x51
    { 0x58, 0x60, 0x62, }, // i = 0x52
    { 0x59, 0x61, 0x63, }, // i = 0x53
    { 0x5A, 0x62, 0x64, }, // i = 0x54
    { 0x5B, 0x63, 0x65, }, // i = 0x55
    { 0x5C, 0x64, 0x66, }, // i = 0x56
    { 0x5D, 0x65, 0x67, }, // i = 0x57
    { 0x5E, 0x66, 0x68, }, // i = 0x58
    { 0x5F, 0x67, 0x69, }, // i = 0x59
    { 0x60, 0x68, 0x6A, }, // i = 0x5A
    { 0x61, 0x69, 0x6B, }, // i = 0x5B
    { 0x62, 0x6A, 0x6D, }, // i = 0x5C
    { 0x63, 0x6B, 0x6E, }, // i = 0x5D
    { 0x64, 0x6C, 0x6F, }, // i = 0x5E
    { 0x65, 0x6D, 0x70, }, // i = 0x5F
    { 0x65, 0x6E, 0x71, }, // i = 0x60
    { 0x66, 0x6F, 0x72, }, // i = 0x61
    { 0x67, 0x70, 0x73, }, // i = 0x62
    { 0x68, 0x71, 0x74, }, // i = 0x63
    { 0x69, 0x72, 0x75, }, // i = 0x64
    { 0x6A, 0x73, 0x76, }, // i = 0x65
    { 0x6B, 0x74, 0x77, }, // i = 0x66
    { 0x6C, 0x75, 0x78, }, // i = 0x67
    { 0x6D, 0x76, 0x79, }, // i = 0x68
    { 0x6E, 0x77, 0x7B, }, // i = 0x69
    { 0x6F, 0x78, 0x7C, }, // i = 0x6A
    { 0x70, 0x79, 0x7D, }, // i = 0x6B
    { 0x71, 0x7A, 0x7E, }, // i = 0x6C
    { 0x72, 0x7B, 0x7F, }, // i = 0x6D
    { 0x73, 0x7C, 0x80, }, // i = 0x6E
    { 0x74, 0x7D, 0x81, }, // i = 0x6F
    { 0x75, 0x7E, 0x82, }, // i = 0x70
    { 0x76, 0x7F, 0x83, }, // i = 0x71
    { 0x77, 0x80, 0x84, }, // i = 0x72
    { 0x78, 0x81, 0x85, }, // i = 0x73
    { 0x79, 0x82, 0x86, }, // i = 0x74
    { 0x7A, 0x83, 0x87, }, // i = 0x75
    { 0x7B, 0x84, 0x88, }, // i = 0x76
    { 0x7B, 0x85, 0x89, }, // i = 0x77
    { 0x7C, 0x86, 0x8A, }, // i = 0x78
    { 0x7D, 0x87, 0x8B, }, // i = 0x79
    { 0x7E, 0x88, 0x8C, }, // i = 0x7A
    { 0x7F, 0x89, 0x8E, }, // i = 0x7B
    { 0x80, 0x8A, 0x8F, }, // i = 0x7C
    { 0x81, 0x8B, 0x90, }, // i = 0x7D
    { 0x82, 0x8C, 0x91, }, // i = 0x7E
    { 0x83, 0x8D, 0x92, }, // i = 0x7F
    { 0x84, 0x8E, 0x93, }, // i = 0x80
    { 0x85, 0x8F, 0x94, }, // i = 0x81
    { 0x86, 0x90, 0x95, }, // i = 0x82
    { 0x87, 0x91, 0x96, }, // i = 0x83
    { 0x88, 0x92, 0x97, }, // i = 0x84
    { 0x89, 0x93, 0x98, }, // i = 0x85
    { 0x8A, 0x94, 0x99, }, // i = 0x86
    { 0x8B, 0x95, 0x9A, }, // i = 0x87
    { 0x8C, 0x96, 0x9B, }, // i = 0x88
    { 0x8D, 0x97, 0x9C, }, // i = 0x89
    { 0x8E, 0x98, 0x9D, }, // i = 0x8A
    { 0x8F, 0x99, 0x9E, }, // i = 0x8B
    { 0x90, 0x9A, 0x9F, }, // i = 0x8C
    { 0x91, 0x9B, 0xA0, }, // i = 0x8D
    { 0x92, 0x9B, 0xA1, }, // i = 0x8E
    { 0x93, 0x9C, 0xA2, }, // i = 0x8F
    { 0x94, 0x9D, 0xA4, }, // i = 0x90
    { 0x95, 0x9E, 0xA5, }, // i = 0x91
    { 0x96, 0x9F, 0xA6, }, // i = 0x92
    { 0x97, 0xA0, 0xA7, }, // i = 0x93
    { 0x98, 0xA1, 0xA8, }, // i = 0x94
    { 0x99, 0xA2, 0xA9, }, // i = 0x95
    { 0x9A, 0xA3, 0xAA, }, // i = 0x96
    { 0x9B, 0xA4, 0xAB, }, // i = 0x97
    { 0x9C, 0xA5, 0xAC, }, // i = 0x98
    { 0x9D, 0xA6, 0xAD, }, // i = 0x99
    { 0x9E, 0xA7, 0xAE, }, // i = 0x9A
    { 0x9F, 0xA8, 0xAF, }, // i = 0x9B
    { 0xA0, 0xA9, 0xB0, }, // i = 0x9C
    { 0xA1, 0xAA, 0xB1, }, // i = 0x9D
    { 0xA2, 0xAB, 0xB2, }, // i = 0x9E
    { 0xA3, 0xAC, 0xB3, }, // i = 0x9F
    { 0xA4, 0xAD, 0xB4, }, // i = 0xA0
    { 0xA5, 0xAE, 0xB5, }, // i = 0xA1
    { 0xA6, 0xAF, 0xB6, }, // i = 0xA2
    { 0xA7, 0xB0, 0xB7, }, // i = 0xA3
    { 0xA8, 0xB1, 0xB8, }, // i = 0xA4
    { 0xA9, 0xB2, 0xB9, }, // i = 0xA5
    { 0xAA, 0xB3, 0xBA, }, // i = 0xA6
    { 0xAA, 0xB4, 0xBB, }, // i = 0xA7
    { 0xAB, 0xB5, 0xBC, }, // i = 0xA8
    { 0xAC, 0xB6, 0xBD, }, // i = 0xA9
    { 0xAD, 0xB7, 0xBE, }, // i = 0xAA
    { 0xAE, 0xB7, 0xBF, }, // i = 0xAB
    { 0xAF, 0xB8, 0xC0, }, // i = 0xAC
    { 0xB0, 0xB9, 0xC1, }, // i = 0xAD
    { 0xB1, 0xBA, 0xC2, }, // i = 0xAE
    { 0xB2, 0xBB, 0xC3, }, // i = 0xAF
    { 0xB3, 0xBC, 0xC4, }, // i = 0xB0
    { 0xB4, 0xBD, 0xC5, }, // i = 0xB1
    { 0xB5, 0xBE, 0xC6, }, // i = 0xB2
    { 0xB6, 0xBF, 0xC7, }, // i = 0xB3
    { 0xB7, 0xC0, 0xC8, }, // i = 0xB4
    { 0xB8, 0xC1, 0xC9, }, // i = 0xB5
    { 0xB9, 0xC2, 0xCA, }, // i = 0xB6
    { 0xBA, 0xC3, 0xCB, }, // i = 0xB7
    { 0xBB, 0xC4, 0xCC, }, // i = 0xB8
    { 0xBC, 0xC5, 0xCD, }, // i = 0xB9
    { 0xBD, 0xC5, 0xCE, }, // i = 0xBA
    { 0xBE, 0xC6, 0xCF, }, // i = 0xBB
    { 0xBF, 0xC7, 0xCF, }, // i = 0xBC
    { 0xC0, 0xC8, 0xD0, }, // i = 0xBD
    { 0xC1, 0xC9, 0xD1, }, // i = 0xBE
    { 0xC2, 0xCA, 0xD2, }, // i = 0xBF
    { 0xC3, 0xCB, 0xD3, }, // i = 0xC0
    { 0xC4, 0xCC, 0xD4, }, // i = 0xC1
    { 0xC5, 0xCD, 0xD5, }, // i = 0xC2
    { 0xC6, 0xCD, 0xD6, }, // i = 0xC3
    { 0xC7, 0xCE, 0xD7, }, // i = 0xC4
    { 0xC7, 0xCF, 0xD8, }, // i = 0xC5
    { 0xC8, 0xD0, 0xD8, }, // i = 0xC6
    { 0xC9, 0xD1, 0xD9, }, // i = 0xC7
    { 0xCA, 0xD2, 0xDA, }, // i = 0xC8
    { 0xCB, 0xD3, 0xDB, }, // i = 0xC9
    { 0xCC, 0xD4, 0xDC, }, // i = 0xCA
    { 0xCD, 0xD4, 0xDD, }, // i = 0xCB
    { 0xCE, 0xD5, 0xDD, }, // i = 0xCC
    { 0xCF, 0xD6, 0xDE, }, // i = 0xCD
    { 0xD0, 0xD7, 0xDF, }, // i = 0xCE
    { 0xD1, 0xD8, 0xE0, }, // i = 0xCF
    { 0xD2, 0xD9, 0xE1, }, // i = 0xD0
    { 0xD3, 0xDA, 0xE1, }, // i = 0xD1
    { 0xD4, 0xDA, 0xE2, }, // i = 0xD2
    { 0xD4, 0xDB, 0xE3, }, // i = 0xD3
    { 0xD5, 0xDC, 0xE4, }, // i = 0xD4
    { 0xD6, 0xDD, 0xE5, }, // i = 0xD5
    { 0xD7, 0xDE, 0xE5, }, // i = 0xD6
    { 0xD8, 0xDF, 0xE6, }, // i = 0xD7
    { 0xD9, 0xDF, 0xE7, }, // i = 0xD8
    { 0xDA, 0xE0, 0xE7, }, // i = 0xD9
    { 0xDB, 0xE1, 0xE8, }, // i = 0xDA
    { 0xDC, 0xE2, 0xE9, }, // i = 0xDB
    { 0xDD, 0xE3, 0xEA, }, // i = 0xDC
    { 0xDE, 0xE3, 0xEA, }, // i = 0xDD
    { 0xDE, 0xE4, 0xEB, }, // i = 0xDE
    { 0xDF, 0xE5, 0xEC, }, // i = 0xDF
    { 0xE0, 0xE6, 0xEC, }, // i = 0xE0
    { 0xE1, 0xE7, 0xED, }, // i = 0xE1
    { 0xE2, 0xE7, 0xEE, }, // i = 0xE2
    { 0xE3, 0xE8, 0xEE, }, // i = 0xE3
    { 0xE4, 0xE9, 0xEF, }, // i = 0xE4
    { 0xE5, 0xEA, 0xF0, }, // i = 0xE5
    { 0xE6, 0xEB, 0xF0, }, // i = 0xE6
    { 0xE7, 0xEB, 0xF1, }, // i = 0xE7
    { 0xE8, 0xEC, 0xF2, }, // i = 0xE8
    { 0xE8, 0xED, 0xF2, }, // i = 0xE9
    { 0xE9, 0xEE, 0xF3, }, // i = 0xEA
    { 0xEA, 0xEF, 0xF4, }, // i = 0xEB
    { 0xEB, 0xEF, 0xF4, }, // i = 0xEC
    { 0xEC, 0xF0, 0xF5, }, // i = 0xED
    { 0xED, 0xF1, 0xF5, }, // i = 0xEE
    { 0xEE, 0xF2, 0xF6, }, // i = 0xEF
    { 0xEF, 0xF3, 0xF7, }, // i = 0xF0
    { 0xF0, 0xF3, 0xF7, }, // i = 0xF1
    { 0xF1, 0xF4, 0xF8, }, // i = 0xF2
    { 0xF2, 0xF5, 0xF8, }, // i = 0xF3
    { 0xF3, 0xF6, 0xF9, }, // i = 0xF4
    { 0xF4, 0xF7, 0xF9, }, // i = 0xF5
    { 0xF5, 0xF7, 0xFA, }, // i = 0xF6
    { 0xF6, 0xF8, 0xFB, }, // i = 0xF7
    { 0xF7, 0xF9, 0xFB, }, // i = 0xF8
    { 0xF8, 0xFA, 0xFC, }, // i = 0xF9
    { 0xF9, 0xFB, 0xFC, }, // i = 0xFA
    { 0xFA, 0xFC, 0xFD, }, // i = 0xFB
    { 0xFB, 0xFC, 0xFE, }, // i = 0xFC
    { 0xFD, 0xFD, 0xFE, }, // i = 0xFD
    { 0xFE, 0xFE, 0xFF, }, // i = 0xFE
    { 0xFF, 0xFF, 0xFF, }, // i = 0xFF
};