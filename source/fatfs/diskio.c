/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2014        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "sdmmc/sdmmc.h"
#include "../crypto.h"

/* Definitions of physical drive number for each media */
#define SDCARD        0
#define CTRNAND       1

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	__attribute__((unused))
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
        static u32 sdmmcInitResult = 4;

        if(sdmmcInitResult == 4) sdmmcInitResult = sdmmc_sdcard_init();

    return ((pdrv == SDCARD && !(sdmmcInitResult & 2)) ||
            (pdrv == CTRNAND && !(sdmmcInitResult & 1) && !ctrNandInit())) ? 0 : STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{
    return ((pdrv == SDCARD && !sdmmc_sdcard_readsectors(sector, count, buff)) ||
            (pdrv == CTRNAND && !ctrNandRead(sector, count, buff))) ? RES_OK : RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,       	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
    return ((pdrv == SDCARD && (*(vu16 *)(SDMMC_BASE + REG_SDSTATUS0) & TMIO_STAT0_WRPROTECT) != 0 && !sdmmc_sdcard_writesectors(sector, count, buff)) ||
            (pdrv == CTRNAND && !ctrNandWrite(sector, count, buff))) ? RES_OK : RES_PARERR;
}
#endif



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT disk_ioctl (
	__attribute__((unused))
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	__attribute__((unused))
	void *buff		/* Buffer to send/receive control data */
)
{
    return cmd == CTRL_SYNC ? RES_OK : RES_PARERR;
}
#endif
