#pragma once
#include <3ds/types.h>
#include "ifile.h"

#define _3GX_MAGIC (0x3130303024584733) /* "3GX$0001" */

typedef struct PACKED
{
    u32             authorLen;
    const char *    authorMsg;
    u32             titleLen;
    const char *    titleMsg;
    u32             summaryLen;
    const char *    summaryMsg;
    u32             descriptionLen;
    const char *    descriptionMsg;
}   _3gx_Infos;

typedef struct PACKED
{
    u32             count;
    u32           * titles;
}   _3gx_Targets;

typedef struct PACKED
{
    u64             magic;
    u32             version;
    u32             codeSize;
    u32           * code;
    _3gx_Infos      infos;
    _3gx_Targets    targets;
}   _3gx_Header;


Result  Read_3gx_Header(IFile *file, _3gx_Header *header);
Result  Read_3gx_Code(IFile *file, _3gx_Header *header, void *dst);
