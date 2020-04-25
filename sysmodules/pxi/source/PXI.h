/*
PXI.h:
    PXI I/O functions.

(c) TuxSH, 2016-2020
This is part of 3ds_pxi, which is licensed under the MIT license (see LICENSE for details).
*/

#pragma once

#include <3ds.h>

#define PXI_REGS_BASE   0x1EC63000
#define REG_PXI_SYNC    *(vu32 *)(PXI_REGS_BASE + 0)
    #define REG_PXI_BYTE_RECEIVED_FROM_REMOTE   *(vu8 *)(PXI_REGS_BASE)
    #define REG_PXI_BYTE_SENT_TO_REMOTE         *(vu8 *)(PXI_REGS_BASE + 1)
    #define REG_PXI_INTERRUPT_CNT               *(vu8 *)(PXI_REGS_BASE + 3)
        #define SYNC_TRIGGER_SYNC9_IRQ  (1U << 6)
        #define SYNC_ENABLE_SYNC11_IRQ  (1U << 7)

#define REG_PXI_CNT     *(vu16 *)(PXI_REGS_BASE + 4)
    #define CNT_SEND_FIFO_FULL_STATUS               (1U <<  1)
    #define CNT_ENABLE_SEND_FIFO_EMPTY_IRQ          (1U <<  2)
    #define CNT_CLEAR_SEND_FIFO                     (1U <<  3)
    #define CNT_RECEIVE_FIFO_EMPTY_STATUS           (1U <<  8)
    #define CNT_ENABLE_RECEIVE_FIFO_NOT_EMPTY_IRQ   (1U << 10)
    #define CNT_ACKNOWLEDGE_FIFO_ERROR              (1U << 14)
    #define CNT_ENABLE_FIFOs                        (1U << 15)

#define REG_PXI_SEND    *(vu32 *)(PXI_REGS_BASE + 8)
#define REG_PXI_RECV    *(vu32 *)(PXI_REGS_BASE + 12)

void PXIReset(void);
void PXITriggerSync9IRQ(void);

bool PXIIsSendFIFOFull(void);
void PXISendByte(u8 byte);
void PXISendWord(u32 word);
void PXISendBuffer(const u32 *buffer, u32 nbWords);

bool PXIIsReceiveFIFOEmpty(void);
u8 PXIReceiveByte(void);
u32 PXIReceiveWord(void);
void PXIReceiveBuffer(u32 *buffer, u32 nbWords);

Result bindPXIInterrupts(Handle *syncInterrupt, Handle *receiveFIFONotEmptyInterrupt, Handle *sendFIFOEmptyInterrupt);
void unbindPXIInterrupts(Handle *syncInterrupt, Handle *receiveFIFONotEmptyInterrupt, Handle *sendFIFOEmptyInterrupt);
