

#pragma once

#include <3ds/types.h>
#include "menu.h"
typedef struct {
    char shortDescription[0x40];
    char longDescription[0x80];
	char productCode[0x10];
	char publisher[0x40];
	u8 largeIcon[0x1200];
	
}Info_Title;

typedef struct {
    u16 shortDescription[0x40];
    u16 longDescription[0x80];
    u16 publisher[0x40];
}SMDH_title;

typedef struct {
    char magic[0x04];
    u16 version;
    u16 reserved1;
    SMDH_title titles[0x10];
    u8 ratings[0x10];
    u32 region;
    u32 matchMakerId;
    u64 matchMakerBitId;
    u32 flags;
    u16 eulaVersion;
    u16 reserved;
    u32 optimalBannerFrame;
    u32 streetpassId;
    u64 reserved2;
    u8 smallIcon[0x480];
    u8 largeIcon[0x1200];
}SMDH;

extern bool reboot;
extern Menu MenuOptions;


void get_Name_TitleID(u64 titleId, u32 count, Info_Title* info);
void CIA_menu(void);
void Delete_Title(void);
Result installCIA(const char *path, FS_MediaType media);

SMDH_title* select_smdh_title(SMDH* smdh);