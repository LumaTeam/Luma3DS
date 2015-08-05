// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common.h"

#include "sdmmc.h"
#include "delay.h"

//Uncomment to enable 32bit fifo support?
//not currently working
//#define DATA32_SUPPORT

static struct mmcdevice handleNAND;
static struct mmcdevice handleSD;

mmcdevice *getMMCDevice(int drive)
{
    if(drive==0) return &handleNAND;
    return &handleSD;
}

int __attribute__((noinline)) geterror(struct mmcdevice *ctx)
{
    //if(ctx->error == 0x4) return -1;
    //else return 0;
    return (ctx->error << 29) >> 31;
}


void __attribute__((noinline)) inittarget(struct mmcdevice *ctx)
{
    sdmmc_mask16(REG_SDPORTSEL,0x3,(u16)ctx->devicenumber);
    setckl(ctx->clk);
    if (ctx->SDOPT == 0) {
        sdmmc_mask16(REG_SDOPT, 0, 0x8000);
    } else {
        sdmmc_mask16(REG_SDOPT, 0x8000, 0);
    }

}


void __attribute__((noinline)) sdmmc_send_command(struct mmcdevice *ctx, u32 cmd, u32 args)
{
    bool getSDRESP = (cmd << 15) >> 31;
    u16 flags = (cmd << 15) >> 31;
    const bool readdata = cmd & 0x20000;
    const bool writedata = cmd & 0x40000;

    if (readdata || writedata)
        flags |= TMIO_STAT0_DATAEND;

    ctx->error = 0;
    while (sdmmc_read16(REG_SDSTATUS1) & TMIO_STAT1_CMD_BUSY); //mmc working?
    sdmmc_write16(REG_SDIRMASK0,0);
    sdmmc_write16(REG_SDIRMASK1,0);
    sdmmc_write16(REG_SDSTATUS0,0);
    sdmmc_write16(REG_SDSTATUS1,0);

#ifdef DATA32_SUPPORT
    if (readdata)
        sdmmc_mask16(REG_SDDATACTL32, 0x1000, 0x800);
    if (writedata)
        sdmmc_mask16(REG_SDDATACTL32, 0x800, 0x1000);
#else
    sdmmc_mask16(REG_SDDATACTL32,0x1800,0);
#endif

    sdmmc_write16(REG_SDCMDARG0,args &0xFFFF);
    sdmmc_write16(REG_SDCMDARG1,args >> 16);
    sdmmc_write16(REG_SDCMD,cmd &0xFFFF);

    u32 size = ctx->size;
    u16 *dataPtr = (u16*)ctx->data;

#ifdef DATA32_SUPPORT
    u32 *dataPtr32 = (u32*)ctx->data;
#endif

    bool useBuf = ( NULL != dataPtr );

#ifdef DATA32_SUPPORT
    bool useBuf32 = (useBuf && (0 == (3 & ((u32)dataPtr))));
#endif

    u16 status0 = 0;
    while(true) {
        u16 status1 = sdmmc_read16(REG_SDSTATUS1);
        if (status1 & TMIO_STAT1_RXRDY) {
            if (readdata && useBuf) {
                sdmmc_mask16(REG_SDSTATUS1, TMIO_STAT1_RXRDY, 0);
                //sdmmc_write16(REG_SDSTATUS1,~TMIO_STAT1_RXRDY);
                if (size > 0x1FF) {
#ifdef DATA32_SUPPORT
                    if (useBuf32) {
                        for(int i = 0; i<0x200; i+=4)
                            *dataPtr32++ = sdmmc_read32(REG_SDFIFO32);
                    } else {
#endif
                        for(int i = 0; i<0x200; i+=2)
                            *dataPtr++ = sdmmc_read16(REG_SDFIFO);
#ifdef DATA32_SUPPORT
                    }
#endif
                    size -= 0x200;
                }
            }
        }

        if (status1 & TMIO_STAT1_TXRQ) {
            if (writedata && useBuf) {
                sdmmc_mask16(REG_SDSTATUS1, TMIO_STAT1_TXRQ, 0);
                //sdmmc_write16(REG_SDSTATUS1,~TMIO_STAT1_TXRQ);
                if (size > 0x1FF) {
#ifdef DATA32_SUPPORT
                    for (int i = 0; i<0x200; i+=4)
                        sdmmc_write32(REG_SDFIFO32,*dataPtr32++);
#else
                    for (int i = 0; i<0x200; i+=2)
                        sdmmc_write16(REG_SDFIFO,*dataPtr++);
#endif
                    size -= 0x200;
                }
            }
        }
        if (status1 & TMIO_MASK_GW) {
            ctx->error |= 4;
            break;
        }

        if (!(status1 & TMIO_STAT1_CMD_BUSY)) {
            status0 = sdmmc_read16(REG_SDSTATUS0);
            if (sdmmc_read16(REG_SDSTATUS0) & TMIO_STAT0_CMDRESPEND)
                ctx->error |= 0x1;
            if (status0 & TMIO_STAT0_DATAEND)
                ctx->error |= 0x2;

            if ((status0 & flags) == flags)
                break;
        }
    }
    ctx->stat0 = sdmmc_read16(REG_SDSTATUS0);
    ctx->stat1 = sdmmc_read16(REG_SDSTATUS1);
    sdmmc_write16(REG_SDSTATUS0,0);
    sdmmc_write16(REG_SDSTATUS1,0);

    if (getSDRESP != 0) {
        ctx->ret[0] = sdmmc_read16(REG_SDRESP0) | (sdmmc_read16(REG_SDRESP1) << 16);
        ctx->ret[1] = sdmmc_read16(REG_SDRESP2) | (sdmmc_read16(REG_SDRESP3) << 16);
        ctx->ret[2] = sdmmc_read16(REG_SDRESP4) | (sdmmc_read16(REG_SDRESP5) << 16);
        ctx->ret[3] = sdmmc_read16(REG_SDRESP6) | (sdmmc_read16(REG_SDRESP7) << 16);
    }
}

int __attribute__((noinline)) sdmmc_sdcard_writesectors(u32 sector_no, u32 numsectors, u8 *in)
{
    if (handleSD.isSDHC == 0)
        sector_no <<= 9;
    inittarget(&handleSD);
    sdmmc_write16(REG_SDSTOP,0x100);

#ifdef DATA32_SUPPORT
    sdmmc_write16(REG_SDBLKCOUNT32,numsectors);
#endif

    sdmmc_write16(REG_SDBLKCOUNT,numsectors);
    handleSD.data = in;
    handleSD.size = numsectors << 9;
    sdmmc_send_command(&handleSD,0x52C19,sector_no);
    return geterror(&handleSD);
}

int __attribute__((noinline)) sdmmc_sdcard_readsectors(u32 sector_no, u32 numsectors, u8 *out)
{
    if (handleSD.isSDHC == 0)
        sector_no <<= 9;
    inittarget(&handleSD);
    sdmmc_write16(REG_SDSTOP,0x100);

#ifdef DATA32_SUPPORT
    sdmmc_write16(REG_SDBLKCOUNT32,numsectors);
    sdmmc_write16(REG_SDBLKLEN32,0x200);
#endif

    sdmmc_write16(REG_SDBLKCOUNT,numsectors);
    handleSD.data = out;
    handleSD.size = numsectors << 9;
    sdmmc_send_command(&handleSD,0x33C12,sector_no);
    return geterror(&handleSD);
}



int __attribute__((noinline)) sdmmc_nand_readsectors(u32 sector_no, u32 numsectors, u8 *out)
{
    if (handleNAND.isSDHC == 0)
        sector_no <<= 9;
    inittarget(&handleNAND);
    sdmmc_write16(REG_SDSTOP,0x100);

#ifdef DATA32_SUPPORT
    sdmmc_write32(REG_SDBLKCOUNT32,numsectors);
#else
    sdmmc_write16(REG_SDBLKCOUNT,numsectors);
#endif

    handleNAND.data = out;
    handleNAND.size = numsectors << 9;
    sdmmc_send_command(&handleNAND,0x33C12,sector_no);
    inittarget(&handleSD);
    return geterror(&handleNAND);
}

int __attribute__((noinline)) sdmmc_nand_writesectors(u32 sector_no, u32 numsectors, u8 *in) //experimental
{
    if (handleNAND.isSDHC == 0)
        sector_no <<= 9;
    inittarget(&handleNAND);
    sdmmc_write16(REG_SDSTOP,0x100);

#ifdef DATA32_SUPPORT
    sdmmc_write32(REG_SDBLKCOUNT32,numsectors);
#else
    sdmmc_write16(REG_SDBLKCOUNT,numsectors);
#endif

    handleNAND.data = in;
    handleNAND.size = numsectors << 9;
    sdmmc_send_command(&handleNAND,0x52C19,sector_no);
    inittarget(&handleSD);
    return geterror(&handleNAND);
}

u32 calcSDSize(u8* csd, int type)
{
    u32 result = 0;
    u8 temp = csd[0xE];
    //int temp3 = type;

    switch (type) {
        case -1:
            type = temp >> 6;
            break;
        case 0:
        {
            u32 temp = (csd[0x7] << 0x2 | csd[0x8] << 0xA | csd[0x6] >> 0x6 | (csd[0x9] & 0xF) << 0x10) & 0xFFF;
            u32 temp2 = temp * (1 << (csd[0x9] & 0xF));
            u32 retval = temp2 * (1 << (((csd[0x4] >> 7 | csd[0x5] << 1) & 7) + 2));
            result = retval >> 9;
            break;
        }
        case 1:
            result = (((csd[0x7] & 0x3F) << 0x10 | csd[0x6] << 8 | csd[0x5]) + 1) << 0xA;
            break;
        default:
            result = 0;
            break;
    }

    return result;
}

void InitSD()
{
    //NAND
    handleNAND.isSDHC = 0;
    handleNAND.SDOPT = 0;
    handleNAND.res = 0;
    handleNAND.initarg = 1;
    handleNAND.clk = 0x80;
    handleNAND.devicenumber = 1;

    //SD
    handleSD.isSDHC = 0;
    handleSD.SDOPT = 0;
    handleSD.res = 0;
    handleSD.initarg = 0;
    handleSD.clk = 0x80;
    handleSD.devicenumber = 0;

    //sdmmc_mask16(0x100,0x800,0);
    //sdmmc_mask16(0x100,0x1000,0);
    //sdmmc_mask16(0x100,0x0,0x402);
    //sdmmc_mask16(0xD8,0x22,0x2);
    //sdmmc_mask16(0x100,0x2,0);
    //sdmmc_mask16(0xD8,0x22,0);
    //sdmmc_write16(0x104,0);
    //sdmmc_write16(0x108,1);
    //sdmmc_mask16(REG_SDRESET,1,0); //not in new Version -- nintendo's code does this
    //sdmmc_mask16(REG_SDRESET,0,1); //not in new Version -- nintendo's code does this
    //sdmmc_mask16(0x20,0,0x31D);
    //sdmmc_mask16(0x22,0,0x837F);
    //sdmmc_mask16(0xFC,0,0xDB);
    //sdmmc_mask16(0xFE,0,0xDB);
    ////sdmmc_write16(REG_SDCLKCTL,0x20);
    ////sdmmc_write16(REG_SDOPT,0x40EE);
    ////sdmmc_mask16(0x02,0x3,0);
    //sdmmc_write16(REG_SDCLKCTL,0x40);
    //sdmmc_write16(REG_SDOPT,0x40EB);
    //sdmmc_mask16(0x02,0x3,0);
    //sdmmc_write16(REG_SDBLKLEN,0x200);
    //sdmmc_write16(REG_SDSTOP,0);

    *(vu16*)0x10006100 &= 0xF7FFu; //SDDATACTL32
    *(vu16*)0x10006100 &= 0xEFFFu; //SDDATACTL32
#ifdef DATA32_SUPPORT
    *(vu16*)0x10006100 |= 0x402u; //SDDATACTL32
#else
    *(vu16*)0x10006100 |= 0x402u; //SDDATACTL32
#endif
    *(vu16*)0x100060D8 = (*(vu16*)0x100060D8 & 0xFFDD) | 2;
#ifdef DATA32_SUPPORT
    *(vu16*)0x10006100 &= 0xFFFFu; //SDDATACTL32
    *(vu16*)0x100060D8 &= 0xFFDFu; //SDDATACTL
    *(vu16*)0x10006104 = 512; //SDBLKLEN32
#else
    *(vu16*)0x10006100 &= 0xFFFDu; //SDDATACTL32
    *(vu16*)0x100060D8 &= 0xFFDDu; //SDDATACTL
    *(vu16*)0x10006104 = 0; //SDBLKLEN32
#endif
    *(vu16*)0x10006108 = 1; //SDBLKCOUNT32
    *(vu16*)0x100060E0 &= 0xFFFEu; //SDRESET
    *(vu16*)0x100060E0 |= 1u; //SDRESET
    *(vu16*)0x10006020 |= TMIO_MASK_ALL; //SDIR_MASK0
    *(vu16*)0x10006022 |= TMIO_MASK_ALL>>16; //SDIR_MASK1
    *(vu16*)0x100060FC |= 0xDBu; //SDCTL_RESERVED7
    *(vu16*)0x100060FE |= 0xDBu; //SDCTL_RESERVED8
    *(vu16*)0x10006002 &= 0xFFFCu; //SDPORTSEL
#ifdef DATA32_SUPPORT
    *(vu16*)0x10006024 = 0x20;
    *(vu16*)0x10006028 = 0x40EE;
#else
    *(vu16*)0x10006024 = 0x40; //Nintendo sets this to 0x20
    *(vu16*)0x10006028 = 0x40EB; //Nintendo sets this to 0x40EE
#endif
    *(vu16*)0x10006002 &= 0xFFFCu; ////SDPORTSEL
    *(vu16*)0x10006026 = 512; //SDBLKLEN
    *(vu16*)0x10006008 = 0; //SDSTOP

    inittarget(&handleSD);
}

int Nand_Init()
{
    inittarget(&handleNAND);
    ioDelay(0xF000);

    sdmmc_send_command(&handleNAND,0,0);

    do {
        do {
            sdmmc_send_command(&handleNAND,0x10701,0x100000);
        } while ( !(handleNAND.error & 1) );
    } while((handleNAND.ret[0] & 0x80000000) == 0);

    sdmmc_send_command(&handleNAND,0x10602,0x0);
    if (handleNAND.error & 0x4) return -1;

    sdmmc_send_command(&handleNAND,0x10403,handleNAND.initarg << 0x10);
    if (handleNAND.error & 0x4) return -1;

    sdmmc_send_command(&handleNAND,0x10609,handleNAND.initarg << 0x10);
    if (handleNAND.error & 0x4) return -1;

    handleNAND.total_size = calcSDSize((u8*)&handleNAND.ret[0],0);
    handleNAND.clk = 1;
    setckl(1);

    sdmmc_send_command(&handleNAND,0x10407,handleNAND.initarg << 0x10);
    if (handleNAND.error & 0x4) return -1;

    handleNAND.SDOPT = 1;

    sdmmc_send_command(&handleNAND,0x10506,0x3B70100);
    if (handleNAND.error & 0x4) return -1;

    sdmmc_send_command(&handleNAND,0x10506,0x3B90100);
    if (handleNAND.error & 0x4) return -1;

    sdmmc_send_command(&handleNAND,0x1040D,handleNAND.initarg << 0x10);
    if (handleNAND.error & 0x4) return -1;

    sdmmc_send_command(&handleNAND,0x10410,0x200);
    if (handleNAND.error & 0x4) return -1;

    handleNAND.clk |= 0x200;

    inittarget(&handleSD);

    return 0;
}

int SD_Init()
{
    inittarget(&handleSD);
    //ioDelay(0x3E8);
    ioDelay(0xF000);
    sdmmc_send_command(&handleSD,0,0);
    sdmmc_send_command(&handleSD,0x10408,0x1AA);
    //u32 temp = (handleSD.ret[0] == 0x1AA) << 0x1E;
    u32 temp = (handleSD.error & 0x1) << 0x1E;

    //int count = 0;
    u32 temp2 = 0;
    do {
        do {
            sdmmc_send_command(&handleSD,0x10437,handleSD.initarg << 0x10);
            sdmmc_send_command(&handleSD,0x10769,0x00FF8000 | temp);
            temp2 = 1;
        } while ( !(handleSD.error & 1) );

    } while((handleSD.ret[0] & 0x80000000) == 0);
    //do
    //{
    //  sdmmc_send_command(&handleSD,0x10437,handleSD.initarg << 0x10);
    //  sdmmc_send_command(&handleSD,0x10769,0x00FF8000 | temp);
    //
    //}
    //while(!(handleSD.ret[0] & 0x80000000));

    if(!((handleSD.ret[0] >> 30) & 1) || !temp)
        temp2 = 0;

    handleSD.isSDHC = temp2;
    //handleSD.isSDHC = (handleSD.ret[0] & 0x40000000);

    sdmmc_send_command(&handleSD,0x10602,0);
    if (handleSD.error & 0x4) return -1;

    sdmmc_send_command(&handleSD,0x10403,0);
    if (handleSD.error & 0x4) return -1;
    handleSD.initarg = handleSD.ret[0] >> 0x10;

    sdmmc_send_command(&handleSD,0x10609,handleSD.initarg << 0x10);
    if (handleSD.error & 0x4) return -1;

    handleSD.total_size = calcSDSize((u8*)&handleSD.ret[0],-1);
    handleSD.clk = 1;
    setckl(1);

    sdmmc_send_command(&handleSD,0x10507,handleSD.initarg << 0x10);
    if (handleSD.error & 0x4) return -1;

    sdmmc_send_command(&handleSD,0x10437,handleSD.initarg << 0x10);
    if (handleSD.error & 0x4) return -1;

    handleSD.SDOPT = 1;
    sdmmc_send_command(&handleSD,0x10446,0x2);
    if (handleSD.error & 0x4) return -1;

    sdmmc_send_command(&handleSD,0x1040D,handleSD.initarg << 0x10);
    if (handleSD.error & 0x4) return -1;

    sdmmc_send_command(&handleSD,0x10410,0x200);
    if (handleSD.error & 0x4) return -1;
    handleSD.clk |= 0x200;

    return 0;
}

void sdmmc_sdcard_init()
{
    InitSD();
    Nand_Init();
    SD_Init();
}
