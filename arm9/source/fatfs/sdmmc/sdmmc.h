#pragma once

#include "../../types.h"

#define SDMMC_BASE		0x10006000

#define REG_SDCMD		0x00
#define REG_SDPORTSEL		0x02
#define REG_SDCMDARG		0x04
#define REG_SDCMDARG0		0x04
#define REG_SDCMDARG1		0x06
#define REG_SDSTOP		0x08
#define REG_SDBLKCOUNT		0x0A

#define REG_SDRESP0		0x0C
#define REG_SDRESP1		0x0E
#define REG_SDRESP2		0x10
#define REG_SDRESP3		0x12
#define REG_SDRESP4		0x14
#define REG_SDRESP5		0x16
#define REG_SDRESP6		0x18
#define REG_SDRESP7		0x1A

#define REG_SDSTATUS0		0x1C
#define REG_SDSTATUS1		0x1E

#define REG_SDIRMASK0		0x20
#define REG_SDIRMASK1		0x22
#define REG_SDCLKCTL		0x24

#define REG_SDBLKLEN		0x26
#define REG_SDOPT		0x28
#define REG_SDFIFO		0x30

#define REG_DATACTL 		0xD8
#define REG_SDRESET		0xE0
#define REG_SDPROTECTED		0xF6 //bit 0 determines if sd is protected or not?

#define REG_DATACTL32 		0x100
#define REG_SDBLKLEN32		0x104
#define REG_SDBLKCOUNT32	0x108
#define REG_SDFIFO32		0x10C

#define REG_CLK_AND_WAIT_CTL	0x138
#define REG_RESET_SDIO		0x1E0

#define TMIO_STAT0_CMDRESPEND    0x0001
#define TMIO_STAT0_DATAEND       0x0004
#define TMIO_STAT0_CARD_REMOVE   0x0008
#define TMIO_STAT0_CARD_INSERT   0x0010
#define TMIO_STAT0_SIGSTATE      0x0020
#define TMIO_STAT0_WRPROTECT     0x0080
#define TMIO_STAT0_CARD_REMOVE_A 0x0100
#define TMIO_STAT0_CARD_INSERT_A 0x0200
#define TMIO_STAT0_SIGSTATE_A    0x0400
#define TMIO_STAT1_CMD_IDX_ERR   0x0001
#define TMIO_STAT1_CRCFAIL       0x0002
#define TMIO_STAT1_STOPBIT_ERR   0x0004
#define TMIO_STAT1_DATATIMEOUT   0x0008
#define TMIO_STAT1_RXOVERFLOW    0x0010
#define TMIO_STAT1_TXUNDERRUN    0x0020
#define TMIO_STAT1_CMDTIMEOUT    0x0040
#define TMIO_STAT1_RXRDY         0x0100
#define TMIO_STAT1_TXRQ          0x0200
#define TMIO_STAT1_ILL_FUNC      0x2000
#define TMIO_STAT1_CMD_BUSY      0x4000
#define TMIO_STAT1_ILL_ACCESS    0x8000

#define TMIO_MASK_ALL           0x837F031D

#define TMIO_MASK_GW            (TMIO_STAT1_ILL_ACCESS | TMIO_STAT1_CMDTIMEOUT | TMIO_STAT1_TXUNDERRUN | TMIO_STAT1_RXOVERFLOW | \
                                 TMIO_STAT1_DATATIMEOUT | TMIO_STAT1_STOPBIT_ERR | TMIO_STAT1_CRCFAIL | TMIO_STAT1_CMD_IDX_ERR)

#define TMIO_MASK_READOP  (TMIO_STAT1_RXRDY | TMIO_STAT1_DATAEND)
#define TMIO_MASK_WRITEOP (TMIO_STAT1_TXRQ | TMIO_STAT1_DATAEND)

typedef struct mmcdevice {
    u8 *rData;
    const u8 *tData;
    u32 size;
    u32 error;
    u16 stat0;
    u16 stat1;
    u32 ret[4];
    u32 initarg;
    u32 isSDHC;
    u32 clk;
    u32 SDOPT;
    u32 devicenumber;
    u32 total_size; //size in sectors of the device
    u32 res;
} mmcdevice;

u32 sdmmc_sdcard_init();
int sdmmc_sdcard_readsectors(u32 sector_no, u32 numsectors, u8 *out);
int sdmmc_sdcard_writesectors(u32 sector_no, u32 numsectors, const u8 *in);
int sdmmc_nand_readsectors(u32 sector_no, u32 numsectors, u8 *out);
int sdmmc_nand_writesectors(u32 sector_no, u32 numsectors, const u8 *in);
void sdmmc_get_cid(bool isNand, u32 *info);
mmcdevice *getMMCDevice(int drive);