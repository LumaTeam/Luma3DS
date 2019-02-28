/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2014-2015, Normmatt
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU General Public License Version 2, as described below:
 *
 * This file is free software: you may copy, redistribute and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or (at your
 * option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/.
 */

#include <stdint.h>
#include <stdbool.h>
#include "wait_cycles.h"
#include "sdmmc.h"

#define DATA32_SUPPORT


struct mmcdevice handleNAND;
struct mmcdevice handleSD;

mmcdevice *getMMCDevice(int drive)
{
    if(drive==0) return &handleNAND;
    return &handleSD;
}

static int get_error(struct mmcdevice *ctx)
{
    return (int)((ctx->error << 29) >> 31);
}


static void set_target(struct mmcdevice *ctx)
{
    sdmmc_mask16(REG_SDPORTSEL,0x3,(u16)ctx->devicenumber);
    setckl(ctx->clk);
    if(ctx->SDOPT == 0)
    {
        sdmmc_mask16(REG_SDOPT,0,0x8000);
    }
    else
    {
        sdmmc_mask16(REG_SDOPT,0x8000,0);
    }
}

static void sdmmc_send_command(struct mmcdevice *ctx, u32 cmd, u32 args)
{
    const bool getSDRESP = (cmd << 15) >> 31;
    u16 flags = (cmd << 15) >> 31;
    const bool readdata = cmd & 0x20000;
    const bool writedata = cmd & 0x40000;

    if(readdata || writedata)
    {
        flags |= TMIO_STAT0_DATAEND;
    }

    ctx->error = 0;
    while((sdmmc_read16(REG_SDSTATUS1) & TMIO_STAT1_CMD_BUSY)); //mmc working?
    sdmmc_write16(REG_SDIRMASK0,0);
    sdmmc_write16(REG_SDIRMASK1,0);
    sdmmc_write16(REG_SDSTATUS0,0);
    sdmmc_write16(REG_SDSTATUS1,0);
    sdmmc_mask16(REG_DATACTL32,0x1800,0x400); // Disable TX32RQ and RX32RDY IRQ. Clear fifo.
    sdmmc_write16(REG_SDCMDARG0,args &0xFFFF);
    sdmmc_write16(REG_SDCMDARG1,args >> 16);
    sdmmc_write16(REG_SDCMD,cmd &0xFFFF);

    u32 size = ctx->size;
    const u16 blkSize = sdmmc_read16(REG_SDBLKLEN32);
    u32 *rDataPtr32 = (u32*)(void*)ctx->rData;
    u8  *rDataPtr8  = ctx->rData;
    const u32 *tDataPtr32 = (u32*)(void*)ctx->tData;
    const u8  *tDataPtr8  = ctx->tData;

    bool rUseBuf = ( NULL != rDataPtr32 );
    bool tUseBuf = ( NULL != tDataPtr32 );

    u16 status0 = 0;
    while(1)
    {
        volatile u16 status1 = sdmmc_read16(REG_SDSTATUS1);
#ifdef DATA32_SUPPORT
        volatile u16 ctl32 = sdmmc_read16(REG_DATACTL32);
        if((ctl32 & 0x100))
#else
        if((status1 & TMIO_STAT1_RXRDY))
#endif
        {
            if(readdata)
            {
                if(rUseBuf)
                {
                    sdmmc_mask16(REG_SDSTATUS1, TMIO_STAT1_RXRDY, 0);
                    if(size >= blkSize)
                    {
                        #ifdef DATA32_SUPPORT
                        if(!((u32)rDataPtr32 & 3))
                        {
                            for(u32 i = 0; i < blkSize; i += 4)
                            {
                                *rDataPtr32++ = sdmmc_read32(REG_SDFIFO32);
                            }
                        }
                        else
                        {
                            for(u32 i = 0; i < blkSize; i += 4)
                            {
                                u32 data = sdmmc_read32(REG_SDFIFO32);
                                *rDataPtr8++ = data;
                                *rDataPtr8++ = data >> 8;
                                *rDataPtr8++ = data >> 16;
                                *rDataPtr8++ = data >> 24;
                            }
                        }
                        #else
                        if(!((u32)rDataPtr16 & 1))
                        {
                            for(u32 i = 0; i < blkSize; i += 4)
                            {
                                *rDataPtr16++ = sdmmc_read16(REG_SDFIFO);
                            }
                        }
                        else
                        {
                            for(u32 i = 0; i < blkSize; i += 4)
                            {
                                u16 data = sdmmc_read16(REG_SDFIFO);
                                *rDataPtr8++ = data;
                                *rDataPtr8++ = data >> 8;
                            }
                        }
                        #endif
                        size -= blkSize;
                    }
                }

                sdmmc_mask16(REG_DATACTL32, 0x800, 0);
            }
        }
#ifdef DATA32_SUPPORT
        if(!(ctl32 & 0x200))
#else
        if((status1 & TMIO_STAT1_TXRQ))
#endif
        {
            if(writedata)
            {
                if(tUseBuf)
                {
                    sdmmc_mask16(REG_SDSTATUS1, TMIO_STAT1_TXRQ, 0);
                    if(size >= blkSize)
                    {
                        #ifdef DATA32_SUPPORT
                        if(!((u32)tDataPtr32 & 3))
                        {
                            for(u32 i = 0; i < blkSize; i += 4)
                            {
                                sdmmc_write32(REG_SDFIFO32, *tDataPtr32++);
                            }
                        }
                        else
                        {
                            for(u32 i = 0; i < blkSize; i += 4)
                            {
                                u32 data = *tDataPtr8++;
                                data |= (u32)*tDataPtr8++ << 8;
                                data |= (u32)*tDataPtr8++ << 16;
                                data |= (u32)*tDataPtr8++ << 24;
                                sdmmc_write32(REG_SDFIFO32, data);
                            }
                        }
                        #else
                        if(!((u32)tDataPtr16 & 1))
                        {
                            for(u32 i = 0; i < blkSize; i += 2)
                            {
                                sdmmc_write16(REG_SDFIFO, *tDataPtr16++);
                            }
                        }
                        else
                        {
                            for(u32 i = 0; i < blkSize; i += 2)
                            {
                                u16 data = *tDataPtr8++;
                                data |= (u16)(*tDataPtr8++ << 8);
                                sdmmc_write16(REG_SDFIFO, data);
                            }
                        }
                        #endif
                        size -= blkSize;
                    }
                }

                sdmmc_mask16(REG_DATACTL32, 0x1000, 0);
            }
        }
        if(status1 & TMIO_MASK_GW)
        {
            ctx->error |= 4;
            break;
        }

        if(!(status1 & TMIO_STAT1_CMD_BUSY))
        {
            status0 = sdmmc_read16(REG_SDSTATUS0);
            if(sdmmc_read16(REG_SDSTATUS0) & TMIO_STAT0_CMDRESPEND)
            {
                ctx->error |= 0x1;
            }
            if(status0 & TMIO_STAT0_DATAEND)
            {
                ctx->error |= 0x2;
            }

            if((status0 & flags) == flags)
                break;
        }
    }
    ctx->stat0 = sdmmc_read16(REG_SDSTATUS0);
    ctx->stat1 = sdmmc_read16(REG_SDSTATUS1);
    sdmmc_write16(REG_SDSTATUS0,0);
    sdmmc_write16(REG_SDSTATUS1,0);

    if(getSDRESP != 0)
    {
        ctx->ret[0] = (u32)(sdmmc_read16(REG_SDRESP0) | (sdmmc_read16(REG_SDRESP1) << 16));
        ctx->ret[1] = (u32)(sdmmc_read16(REG_SDRESP2) | (sdmmc_read16(REG_SDRESP3) << 16));
        ctx->ret[2] = (u32)(sdmmc_read16(REG_SDRESP4) | (sdmmc_read16(REG_SDRESP5) << 16));
        ctx->ret[3] = (u32)(sdmmc_read16(REG_SDRESP6) | (sdmmc_read16(REG_SDRESP7) << 16));
    }
}

int sdmmc_sdcard_writesectors(u32 sector_no, u32 numsectors, const u8 *in)
{
    if(handleSD.isSDHC == 0) sector_no <<= 9;
    set_target(&handleSD);
    sdmmc_write16(REG_SDSTOP,0x100);
#ifdef DATA32_SUPPORT
    sdmmc_write16(REG_SDBLKCOUNT32,numsectors);
    sdmmc_write16(REG_SDBLKLEN32,0x200);
#endif
    sdmmc_write16(REG_SDBLKCOUNT,numsectors);
    handleSD.tData = in;
    handleSD.size = numsectors << 9;
    sdmmc_send_command(&handleSD,0x52C19,sector_no);
    return get_error(&handleSD);
}

int sdmmc_sdcard_readsectors(u32 sector_no, u32 numsectors, u8 *out)
{
    if(handleSD.isSDHC == 0) sector_no <<= 9;
    set_target(&handleSD);
    sdmmc_write16(REG_SDSTOP,0x100);
#ifdef DATA32_SUPPORT
    sdmmc_write16(REG_SDBLKCOUNT32,numsectors);
    sdmmc_write16(REG_SDBLKLEN32,0x200);
#endif
    sdmmc_write16(REG_SDBLKCOUNT,numsectors);
    handleSD.rData = out;
    handleSD.size = numsectors << 9;
    sdmmc_send_command(&handleSD,0x33C12,sector_no);
    return get_error(&handleSD);
}



int sdmmc_nand_readsectors(u32 sector_no, u32 numsectors, u8 *out)
{
    if(handleNAND.isSDHC == 0) sector_no <<= 9;
    set_target(&handleNAND);
    sdmmc_write16(REG_SDSTOP,0x100);
#ifdef DATA32_SUPPORT
    sdmmc_write16(REG_SDBLKCOUNT32,numsectors);
    sdmmc_write16(REG_SDBLKLEN32,0x200);
#endif
    sdmmc_write16(REG_SDBLKCOUNT,numsectors);
    handleNAND.rData = out;
    handleNAND.size = numsectors << 9;
    sdmmc_send_command(&handleNAND,0x33C12,sector_no);
    return get_error(&handleNAND);
}

int sdmmc_nand_writesectors(u32 sector_no, u32 numsectors, const u8 *in) //experimental
{
    if(handleNAND.isSDHC == 0) sector_no <<= 9;
    set_target(&handleNAND);
    sdmmc_write16(REG_SDSTOP,0x100);
#ifdef DATA32_SUPPORT
    sdmmc_write16(REG_SDBLKCOUNT32,numsectors);
    sdmmc_write16(REG_SDBLKLEN32,0x200);
#endif
    sdmmc_write16(REG_SDBLKCOUNT,numsectors);
    handleNAND.tData = in;
    handleNAND.size = numsectors << 9;
    sdmmc_send_command(&handleNAND,0x52C19,sector_no);
    return get_error(&handleNAND);
}

static u32 sdmmc_calc_size(u8* csd, int type)
{
  u32 result = 0;
  if(type == -1) type = csd[14] >> 6;
  switch(type)
  {
    case 0:
      {
        u32 block_len=csd[9]&0xf;
        block_len=1u<<block_len;
        u32 mult=( u32)((csd[4]>>7)|((csd[5]&3)<<1));
        mult=1u<<(mult+2);
        result=csd[8]&3;
        result=(result<<8)|csd[7];
        result=(result<<2)|(csd[6]>>6);
        result=(result+1)*mult*block_len/512;
      }
      break;
    case 1:
      result=csd[7]&0x3f;
      result=(result<<8)|csd[6];
      result=(result<<8)|csd[5];
      result=(result+1)*1024;
      break;
    default:
      break; //Do nothing otherwise FIXME perhaps return some error?
  }
  return result;
}

void sdmmc_init()
{
    //NAND
    handleNAND.isSDHC = 0;
    handleNAND.SDOPT = 0;
    handleNAND.res = 0;
    handleNAND.initarg = 1;
    handleNAND.clk = 0x20; // 523.655968 KHz
    handleNAND.devicenumber = 1;

    //SD
    handleSD.isSDHC = 0;
    handleSD.SDOPT = 0;
    handleSD.res = 0;
    handleSD.initarg = 0;
    handleSD.clk = 0x20; // 523.655968 KHz
    handleSD.devicenumber = 0;

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
    *(vu16*)0x10006028 = 0x40E9;
#else
    *(vu16*)0x10006024 = 0x40; //Nintendo sets this to 0x20
    *(vu16*)0x10006028 = 0x40E9; //Nintendo sets this to 0x40EE
#endif
    *(vu16*)0x10006002 &= 0xFFFCu; ////SDPORTSEL
    *(vu16*)0x10006026 = 512; //SDBLKLEN
    *(vu16*)0x10006008 = 0; //SDSTOP
}

int Nand_Init()
{
    // init the handle
    handleNAND.isSDHC = 0;
    handleNAND.SDOPT = 0;
    handleNAND.res = 0;
    handleNAND.initarg = 1;
    handleNAND.clk = 0x20; // 523.655968 KHz
    handleNAND.devicenumber = 1;

    // The eMMC is always on. Nothing special to do.
    set_target(&handleNAND);

    sdmmc_send_command(&handleNAND,0,0);

    do
    {
        do
        {
            sdmmc_send_command(&handleNAND,0x10701,0x100000);
        } while ( !(handleNAND.error & 1) );
    }
    while((handleNAND.ret[0] & 0x80000000) == 0);

    sdmmc_send_command(&handleNAND,0x10602,0x0);
    if((handleNAND.error & 0x4))return -1;

    sdmmc_send_command(&handleNAND,0x10403,handleNAND.initarg << 0x10);
    if((handleNAND.error & 0x4))return -1;

    sdmmc_send_command(&handleNAND,0x10609,handleNAND.initarg << 0x10);
    if((handleNAND.error & 0x4))return -1;

    handleNAND.total_size = sdmmc_calc_size((u8*)&handleNAND.ret[0],0);
    setckl(0x201); // 16.756991 MHz

    sdmmc_send_command(&handleNAND,0x10407,handleNAND.initarg << 0x10);
    if((handleNAND.error & 0x4))return -1;

    handleNAND.SDOPT = 1;
    sdmmc_send_command(&handleNAND,0x10506,0x3B70100); // Set 4 bit bus width.
    if((handleNAND.error & 0x4))return -1;
    sdmmc_mask16(REG_SDOPT, 0x8000, 0); // Switch to 4 bit mode.

    sdmmc_send_command(&handleNAND,0x10506,0x3B90100); // Switch to high speed timing.
    if((handleNAND.error & 0x4))return -1;
    handleNAND.clk = 0x200; // 33.513982 MHz
    setckl(0x200);

    sdmmc_send_command(&handleNAND,0x1040D,handleNAND.initarg << 0x10);
    if((handleNAND.error & 0x4))return -1;

    sdmmc_send_command(&handleNAND,0x10410,0x200);
    if((handleNAND.error & 0x4))return -1;

    return 0;
}

int SD_Init()
{
    // init the handle
    handleSD.isSDHC = 0;
    handleSD.SDOPT = 0;
    handleSD.res = 0;
    handleSD.initarg = 0;
    handleSD.clk = 0x20; // 523.655968 KHz
    handleSD.devicenumber = 0;

    // We need to send at least 74 clock pulses.
    set_target(&handleSD);
    wait_cycles(0x1980); // ~75-76 clocks

    sdmmc_send_command(&handleSD,0,0);
    sdmmc_send_command(&handleSD,0x10408,0x1AA);
    u32 temp = (handleSD.error & 0x1) << 0x1E;

    u32 temp2 = 0;

    do
    {
        do
        {
            sdmmc_send_command(&handleSD,0x10437,handleSD.initarg << 0x10);
            sdmmc_send_command(&handleSD,0x10769,0x10100000 | temp); // Allow 150mA, 3.2-3.3V (from Process9)
            temp2 = 1;
        } while ( !(handleSD.error & 1) );
    }
    while((handleSD.ret[0] & 0x80000000) == 0);

    if(!((handleSD.ret[0] >> 30) & 1) || !temp)
        temp2 = 0;

    handleSD.isSDHC = temp2;

    sdmmc_send_command(&handleSD,0x10602,0);
    if((handleSD.error & 0x4)) return -1;

    sdmmc_send_command(&handleSD,0x10403,0);
    if((handleSD.error & 0x4)) return -2;
    handleSD.initarg = handleSD.ret[0] >> 0x10;

    sdmmc_send_command(&handleSD,0x10609,handleSD.initarg << 0x10);
    if((handleSD.error & 0x4)) return -3;

    // Command Class 10 support
    const bool cmd6Supported = ((u8*)handleSD.ret)[10] & 0x40;
    handleSD.total_size = sdmmc_calc_size((u8*)&handleSD.ret[0],-1);
    setckl(0x201); // 16.756991 MHz

    sdmmc_send_command(&handleSD,0x10507,handleSD.initarg << 0x10);
    if((handleSD.error & 0x4)) return -4;

    // CMD55
    sdmmc_send_command(&handleSD,0x10437,handleSD.initarg << 0x10);
    if(handleSD.error & 0x4) return -5;

    // ACMD42 SET_CLR_CARD_DETECT
    sdmmc_send_command(&handleSD,0x1076A,0x0);
    if(handleSD.error & 0x4) return -6;

    sdmmc_send_command(&handleSD,0x10437,handleSD.initarg << 0x10);
    if((handleSD.error & 0x4)) return -7;

    handleSD.SDOPT = 1;
    sdmmc_send_command(&handleSD,0x10446,0x2);
    if((handleSD.error & 0x4)) return -8;
    sdmmc_mask16(REG_SDOPT, 0x8000, 0); // Switch to 4 bit mode.

    // TODO: CMD6 to switch to high speed mode.
    if(cmd6Supported)
    {
        sdmmc_write16(REG_SDSTOP,0);
        sdmmc_write16(REG_SDBLKLEN32,64);
        sdmmc_write16(REG_SDBLKLEN,64);
        handleSD.rData = NULL;
        handleSD.size = 64;
        sdmmc_send_command(&handleSD,0x31C06,0x80FFFFF1);
        sdmmc_write16(REG_SDBLKLEN,512);
        if(handleSD.error & 0x4) return -9;

        handleSD.clk = 0x200; // 33.513982 MHz
        setckl(0x200);
    }
    else handleSD.clk = 0x201; // 16.756991 MHz

    sdmmc_send_command(&handleSD,0x1040D,handleSD.initarg << 0x10);
    if((handleSD.error & 0x4)) return -9;

    sdmmc_send_command(&handleSD,0x10410,0x200);
    if((handleSD.error & 0x4)) return -10;

    return 0;
}

int sdmmc_get_cid(bool isNand, u32 *info)
{
    struct mmcdevice *device;
    if(isNand)
        device = &handleNAND;
    else
        device = &handleSD;

    set_target(device);
    // use cmd7 to put sd card in standby mode
    // CMD7
    {
        sdmmc_send_command(device,0x10507,0);
        //if((device->error & 0x4)) return -1;
    }

    // get sd card info
    // use cmd10 to read CID
    {
        sdmmc_send_command(device,0x1060A,device->initarg << 0x10);
        //if((device->error & 0x4)) return -2;

        for( int i = 0; i < 4; ++i ) {
            info[i] = device->ret[i];
        }
    }

    // put sd card back to transfer mode
    // CMD7
    {
        sdmmc_send_command(device,0x10507,device->initarg << 0x10);
        //if((device->error & 0x4)) return -3;
    }

    return 0;
}

u32 sdmmc_sdcard_init()
{
    u32 ret = 0;

    // SD mount fix
    *((vu16*)0x10000020) = 0x340;

    // init SDMMC / NAND
    sdmmc_init();
    if(Nand_Init() != 0) ret &= 1;

    // init SDMMC / SDCARD
    u32 timeout = 20; // number of tries (2ms per try)

    do {
        // if sd card is ready, stop polling
        if(sdmmc_read16(REG_SDSTATUS0) & TMIO_STAT0_SIGSTATE)
            break;

        wait_cycles(268000000); // approx 2ms
        timeout--;
    } while(timeout);

    if(!timeout || SD_Init() != 0) ret &= 2;

    return ret;
}
