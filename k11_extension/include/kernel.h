/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#pragma once

#include "types.h"
#include <string.h>

extern u32 kernelVersion;

struct KMutex;
struct KProcessO3DS;
struct KProcessN3DS;
union  KProcess;
struct HandleDescriptor;
struct KDebugThread;
struct KCodeSet;
struct KDebug;
struct KResourceLimit;
struct KPort;
struct KSession;
struct KLinkedListNode;
struct KThread;
struct Vtable__KAutoObject;
struct KClassToken;
struct Vtable__KBaseInterruptEvent;
struct KObjectMutex;
struct KThreadLinkedList;
struct KMutexLinkedList;
struct KPreemptionTimer;

/* 12 */
typedef struct ALIGN(4) KAutoObject
{
  struct Vtable__KAutoObject *vtable;
  u32 refCount;
} KAutoObject;

/* 10 */
typedef struct KLinkedListNode
{
  struct KLinkedListNode *next;
  struct KLinkedListNode *prev;
  void *key;
} KLinkedListNode;

/* 83 */
typedef struct KLinkedListNodePair
{
  KLinkedListNode *first;
  KLinkedListNode *last;
} KLinkedListNodePair;

/* 11 */
typedef struct KLinkedList
{
  u32 size;
  KLinkedListNodePair nodes;
} KLinkedList;

/* 13 */
typedef struct KSynchronizationObject
{
  KAutoObject autoObject;
  KLinkedList syncedThreads;
} KSynchronizationObject;

/* 91 */
typedef struct KMutexLinkedListNode
{
  struct KMutex *prev;
  struct KMutex *next;
} KMutexLinkedListNode;

/* 1 */
typedef struct ALIGN(4) KMutex
{
  KSynchronizationObject syncObject;
  KMutexLinkedListNode mutexListNode;
  u32 nbThreadsUsingThis;
  struct KThread *lockingThread;
  u32 priority;
  union KProcess *owner;
} KMutex;

typedef struct KAddressArbiter
{
  KAutoObject     autoObject;
  struct KThread  *first;
  struct KThread  *last;
  union  KProcess *owner;
} KAddressArbiter;

/* 92 */
typedef struct KMutexLinkedList
{
  KMutex *first;
  KMutex *last;
} KMutexLinkedList;

enum
{
    TOKEN_KAUTOOBJECT = 0,
    TOKEN_KSYNCHRONIZATIONOBJECT = 1,
    TOKEN_KEVENT = 0x1F,
    TOKEN_KSEMAPHORE = 0x2F,
    TOKEN_KTIMER = 0x35,
    TOKEN_KMUTEX = 0x39,
    TOKEN_KDEBUG = 0x4D,
    TOKEN_KSERVERPORT = 0x55,
    TOKEN_KDMAOBJECT = 0x59,
    TOKEN_KCLIENTPORT = 0x65,
    TOKEN_KCODESET = 0x68,
    TOKEN_KSESSION = 0x70,
    TOKEN_KTHREAD = 0x8D,
    TOKEN_KSERVERSESSION = 0x95,
    TOKEN_KADDRESSARBITER = 0x98,
    TOKEN_KCLIENTSESSION = 0xA5,
    TOKEN_KPORT = 0xA8,
    TOKEN_KSHAREDMEMORY = 0xB0,
    TOKEN_KPROCESS = 0xC5,
    TOKEN_KRESOURCELIMIT = 0xC8
};

/* 45 */
typedef struct KClassToken
{
  const char *name;
  u8 flags;
} KClassToken;

/* 44 */
typedef struct ALIGN(4) Vtable__KAutoObject
{
  void *field_0;
  void *field_4;
  void (*dtor)(KAutoObject *this);
  void *substractResource;
  KAutoObject *(*DecrementReferenceCount)(KAutoObject *this);
  union KProcess *(*GetParentProcess)(KAutoObject *this);
  KClassToken *(*GetClassToken)(KClassToken *out, KAutoObject *this); // >= 9.0 only
  const char *(*GetClassName)(KAutoObject *this); // < 9.0 only
  void *field_20;
  void *field_24;
} Vtable__KAutoObject;

/* 52 */
typedef struct KBaseInterruptEvent
{
  struct Vtable__KBaseInterruptEvent *vtable;
} KBaseInterruptEvent;

/* 55 */
typedef struct ALIGN(4) Vtable__KBaseInterruptEvent
{
  struct KSchedulableInterruptEvent *(*handleInterruptEvent)(KBaseInterruptEvent *, u32);
} Vtable__KBaseInterruptEvent;


/* 67 */
typedef struct KSynchronizationInterruptEvent
{
  KBaseInterruptEvent baseInterruptEvent;
  struct KSynchronizationInterruptEvent *next;
} KSynchronizationInterruptEvent;

/* 77 */
// This one got added a vtable func on 11.3
typedef struct KTimeableInterruptEvent
{
  KSynchronizationInterruptEvent syncInterruptEvent;
  s64 timer;
} KTimeableInterruptEvent;

/* 90 */
typedef KSynchronizationInterruptEvent KSendableInterruptEvent;

/* 68 */
typedef KSynchronizationInterruptEvent KSchedulableInterruptEvent;

/* 85 */
typedef struct KThreadLinkedListNode
{
  struct KThread *prev;
  struct KThread *next;
} KThreadLinkedListNode;


/* 93 */
typedef struct ALIGN(4) KPreemptionTimer
{
  u32 nLimitedTicks;
  u32 timer;
  u32 previousWDTValue;
  u32 wantedDuration;
} KPreemptionTimer;

/* 15 */
typedef struct PACKED ALIGN(4) KThread
{
  KSynchronizationObject syncObject;
  KTimeableInterruptEvent timeableInterruptEvent;
  KSendableInterruptEvent sendableInterruptEvent;
  KSchedulableInterruptEvent schedulableInterruptEvent;
  u8 schedulingMask;
  u8 syncing_1;
  bool shallTerminate;
  u8 syncing_3;
  struct KDebugThread *debugThread;
  u32 basePriority;
  KSynchronizationObject *objectThreadIsWaitingOn;
  Result resultObjectWaited_2;
  struct KObjectMutex *mutex;
  void *arbitrationAddr;
  KLinkedList objectsToWaitFor;
  KMutexLinkedList *mutexList;
  KLinkedList mutexesUsed;
  s32 dynamicPriority;
  u32 coreId;
  KPreemptionTimer *preemptionTimer;
  u32 unknown_1;
  bool isAlive;
  bool isEnded;
  u8 affinityMask, padding;
  union KProcess *ownerProcess;
  u32 threadId;
  void *svcRegisterStorage;
  void *endOfThreadContext;
  s32 idealProcessor;
  void *threadLocalStorage;
  void *threadLocalStorageKernel;
  void *unknown_5;
  KThreadLinkedListNode threadListNode;
  struct KThreadLinkedList *stolenThreadList;
  s32 highestPriority;
} KThread;

/* 79 */
typedef struct KObjectMutex
{
  KThread *owner;
  s16 tryAcquireCounter;
  s16 contextSwitchCounter;
} KObjectMutex;



/* 120 */
typedef enum ProcessStatus
{
  PROCESS_AVAILABLE = 0x1,
  PROCESS_CLOSING = 0x2,
  PROCESS_EXITED = 0x3,
} ProcessStatus;

/* 3 */
typedef struct ALIGN(4) HandleDescriptor
{
  u32 info;
  KAutoObject *pointer;
} HandleDescriptor;

/* 22 */
typedef struct KProcessHandleTable
{
  HandleDescriptor *handleTable;
  s16 maxHandleCount;
  s16 highestHandleCount;
  HandleDescriptor *nextOpenHandleDescriptor;
  s16 totalHandlesUsed;
  s16 handlesInUseCount;
  KObjectMutex mutex;
  HandleDescriptor internalTable[40];
} KProcessHandleTable;

/* 4 */
typedef struct ALIGN(4) KDebugThread
{
  KThread *linkedThread;
  bool usedSvcBreak;
  bool debugLocked;
  bool shallBeDebugLocked;
  bool scheduledInOrOut;
  bool threadAttached;
  bool syscallInOurOut;
  u8 paddingA[2];
  u32 threadExitReason;
  u32 creatorThreadId;
  u32 FAR;
} KDebugThread;

/* 84 */
typedef struct KThreadLinkedList
{
  KThread *first;
  KThread *last;
} KThreadLinkedList;

typedef enum {
  DBGEVENT_ATTACH_PROCESS = 0,  ///< Process attached event.
  DBGEVENT_ATTACH_THREAD  = 1,  ///< Thread attached event.
  DBGEVENT_EXIT_THREAD    = 2,  ///< Thread exit event.
  DBGEVENT_EXIT_PROCESS   = 3,  ///< Process exit event.
  DBGEVENT_EXCEPTION      = 4,  ///< Exception event.
  DBGEVENT_DLL_LOAD       = 5,  ///< DLL load event.
  DBGEVENT_DLL_UNLOAD     = 6,  ///< DLL unload event.
  DBGEVENT_SCHEDULE_IN    = 7,  ///< Schedule in event.
  DBGEVENT_SCHEDULE_OUT   = 8,  ///< Schedule out event.
  DBGEVENT_SYSCALL_IN     = 9,  ///< Syscall in event.
  DBGEVENT_SYSCALL_OUT    = 10, ///< Syscall out event.
  DBGEVENT_OUTPUT_STRING  = 11, ///< Output string event.
  DBGEVENT_MAP            = 12, ///< Map event.
} DebugEventType;

/* 80 */
typedef struct KRecursiveLock
{
  KThread *owner;
  u32 lockCount;
} KRecursiveLock;

typedef enum {
  EXCEVENT_UNDEFINED_INSTRUCTION = 0, ///< Undefined instruction.
  EXCEVENT_PREFETCH_ABORT        = 1, ///< Prefetch abort.
  EXCEVENT_DATA_ABORT            = 2, ///< Data abort (other than the below kind).
  EXCEVENT_UNALIGNED_DATA_ACCESS = 3, ///< Unaligned data access.
  EXCEVENT_ATTACH_BREAK          = 4, ///< Attached break.
  EXCEVENT_STOP_POINT            = 5, ///< Stop point reached.
  EXCEVENT_USER_BREAK            = 6, ///< User break occurred.
  EXCEVENT_DEBUGGER_BREAK        = 7, ///< Debugger break occurred.
  EXCEVENT_UNDEFINED_SYSCALL     = 8, ///< Undefined syscall.
} ExceptionEventType;

/* 6 */
typedef struct ALIGN(4) KDebug
{
  KSynchronizationObject syncObject;
  KSendableInterruptEvent sendableInterruptEvent;
  bool processDebugEventSignaled;
  bool debugStringFlag;
  bool isSignalingDebugEvent;
  bool isPaused;
  DebugEventType debugEventType;
  u32 field_24;
  KThread *lockingThread;
  u32 debugEventFlags;
  u32 stopPointType;
  KLinkedList eventInfos;
  KLinkedList infoForEventsToContinue;
  union KProcess *owner;
  KThread *debuggedThread;
  KThread *threadUsingContinueDebugEvent;
  u32 coreIDOfThreadUsingContinueDebugEvent;
  KLinkedList debuggedThreads;
  KLinkedList threadsOfProcess;
  KRecursiveLock recursiveLock;
  KThread *threadUsingSvcBreak;
  bool noExceptionDebugEventOccured;
  bool exceptionDebugEventSignaled;
  bool shouldUnlockAll;
  bool exceptionDebugEventOccuredWhilePaused;
  bool userBreakEventOccuredWhilePaused;
  bool processHasEnded;
  bool processHasBeenTerminated;
  bool pendingSvcBreakEvent;
  u32 *regdump;
  ExceptionEventType exceptionInfo;
  u16 nbExceptions;
  u16 svcID;
  u16 nbPendingEvents;
  u16 nbPendingExitProcessEvents;
  const char *debugString;
  u32 debugStringLength;
} KDebug;

/* 7 */
typedef struct KResourceLimit
{
  KAutoObject autoObject;
  s32 maxResources[10];
  s32 currentResources[10];
  KObjectMutex objMutex;
  KPreemptionTimer preemptionTimer;
  u32 padding;
} KResourceLimit;

/* 64 */
typedef struct SectionInfo
{
  void *loadAddress;
  u32 nbPages;
} SectionInfo;

/* 23 */
typedef struct KCodeSetMemDescriptor
{
  SectionInfo section;
  struct KLinkedList blocks;
} KCodeSetMemDescriptor;

/* 5 */
typedef struct PACKED ALIGN(4) KCodeSet
{
  KAutoObject autoObject;
  KCodeSetMemDescriptor textSection;
  KCodeSetMemDescriptor rodataSection;
  KCodeSetMemDescriptor dataSection;
  u32 nbTextPages;
  u32 nbRodataPages;
  u32 nbRwPages;
  char processName[8];
  u16 unknown_1;
  u16 unknown_2;
  u64 titleId;
} KCodeSet;

/* 30 */
typedef struct KServerPort
{
  KSynchronizationObject syncObject;
  KLinkedList serverSessions;
  struct KPort *parentKPort;
} KServerPort;

/* 31 */
typedef struct KClientPort
{
  KSynchronizationObject syncObject;
  s16 connectionCount;
  s16 maxConnectionCount;
  struct KPort *parentKPort;
} KClientPort;

/* 8 */
typedef struct KPort
{
  KAutoObject autoObject;
  KServerPort serverPort;
  KClientPort clientPort;
} KPort;

/* 33 */
typedef struct KServerSession
{
  KSynchronizationObject syncObject;
  struct KSession *parentSession;
  KThread *lastStolenThread;
  KThread *firstStolenThread;
  KThread *originatingThread;
} KServerSession;

/* 32 */
typedef struct KClientSession
{
  KSynchronizationObject syncObject;
  struct KSession *parentSession;
  u32 status;
  KClientPort *clientPort;
} KClientSession;

/* 9 */
typedef struct KSession
{
  KAutoObject autoObject;
  KServerSession serverSession;
  KClientSession clientSession;
} KSession;

/* 82 */
typedef struct KUserBindableInterruptEvent
{
  KSchedulableInterruptEvent schedulableInterruptEvent;
  s32 interruptID;
} KUserBindableInterruptEvent;

/* 14 */
typedef struct ALIGN(4) KEvent
{
  KSynchronizationObject syncObject;
  KUserBindableInterruptEvent userBindableInterruptEvent;
  bool isSignaled;
  bool manualClear;
  u8 resetType;
  u8 unused;
  union KProcess *owner;
} KEvent;

/* 16 */
typedef enum MemOp
{
  MEMOP_FREE = 0x1,
  MEMOP_RESERVE = 0x2,
  MEMOP_COMMIT = 0x3,
  MEMOP_MAP = 0x4,
  MEMOP_UNMAP = 0x5,
  MEMOP_PROTECT = 0x6,
  MEMOP_REGION_APP = 0x100,
  MEMOP_REGION_SYSTEM = 0x200,
  MEMOP_REGION_BASE = 0x300,
  MEMOP_LINEAR = 0x10000,

  MEMOP_OP_MASK = 0xFF,
  MEMOP_REGION_MASK = 0xF00,
} MemOp;

/* 17 */
typedef enum MemPerm
{
  MEMPERM_NONE = 0x0,
  MEMPERM_R = 0x1,
  MEMPERM_W = 0x2,
  MEMPERM_RW = 0x3,
  MEMPERM_X = 0x4,
  MEMPERM_RX = 0x5,
  MEMPERM_WX = 0x6,
  MEMPERM_RWX = 0x7,
  MEMPERM_DONTCARE = 0x10000000,
} MemPerm;

/* 18 */
typedef struct KObjectLink
{
  void *nextAvailable;
  void *first;
  u32 kheapAllocSize;
  u32 size;
} KObjectLink;

/* 19 */
typedef struct KObjectList
{
  KLinkedList list;
  KObjectMutex mutex;
  s16 totalObjectsCreated;
  s16 padding2;
  KObjectLink objectInfo;
} KObjectList;

/* 21 */
typedef struct KSemaphore
{
  KSynchronizationObject syncObject;
  KUserBindableInterruptEvent userBindableInterruptEvent;
  u32 count;
  u32 maxCount;
  union KProcess *owner;
} KSemaphore;

/* 24 */
typedef struct KBlockInfo
{
  void *memSectionStartKVaddr;
  u32 pageCount;
} KBlockInfo;

typedef struct KSharedMemory
{
  KAutoObject   autoObject;
  KLinkedList   ownedKBlockInfo;
  union KProcess *owner;
  u32           ownerPermissions;
  u32           otherPermissions;
  u8            isBlockInfoGenerated;
  s8            allBlockInfoGenerated;
  u8            unknown_1;
  u8            unknown_2;
  u32           address;
} KSharedMemory;

/* 25 */
typedef struct KMemoryBlock
{
  void *baseAddr;
  u32 sizeInPages;
  u32 permissions;
  u32 state;
  u32 zero;
} KMemoryBlock;

/* 28 */
typedef struct ALIGN(4) KScheduler
{
  KSchedulableInterruptEvent interruptEvent;
  u32 threadSwitchAttempts;
  bool contextSwitchNeeded;
  bool contextSwitchStartedDuringInterrupt;
  bool triggerCrossCoreInterrupt;
  bool postInterruptReschedulingNeeded;
  s16 coreNumber;
  s16 managedThreadsCount;
  u32 highPriorityThreadsBitfield;
  u32 lowPriorityThreadsBitfield;
  KThread *schedulerThread;
  KThreadLinkedList listOfHandledThreads;
  KThreadLinkedList threadsByPriority[64];
} KScheduler;

/* 46 */
typedef struct PACKED CodeSetInfo
{
  char name[8];
  u16 unknown_1;
  u16 unknown_2;
  u32 unknown_3;
  void *textAddr;
  u32 textSize;
  void *rodataAddr;
  u32 rodataSize;
  void *rwAddr;
  u32 rwSize;
  u32 totalTextPages;
  u32 totalRodataPages;
  u32 totalRwPages;
  u32 unknown_4;
  u64 programId;
} CodeSetInfo;

/* 53 */
typedef struct ALIGN(4) InterruptData
{
  KBaseInterruptEvent *interruptEvent;
  bool disableUponReceipt;
  bool isDisabled;
  u8 priority;
  u8 padding;
} InterruptData;

/* 59 */
typedef struct KFIQInterruptEvent
{
  KSchedulableInterruptEvent schedulableInterruptEvent;
  KEvent *event;
  bool isLevelHighActive;
  bool otherFlag;
} KFIQInterruptEvent;

/* 61 */
typedef enum TLBOperation
{
  NO_TLB_OPERATION = 0x0,
  FLUSH_TLB_ENTRIES_ON_ASID_MATCH = 0x1,
  FLUSH_ENTIRE_TLB = 0x2,
  FLUSH_TLB_ENTRIES_ON_MVA_MATCH = 0x3,
  TLB_RESET_CONTEXT = 0x4,
} TLBOperation;

/* 63 */
typedef struct TLBOperationTarget
{
  union KProcessHwInfo *contextInfo;
  void *VA;
} TLBOperationTarget;

/* 62 */
typedef struct KTLBOperationsInterruptEvent
{
  KBaseInterruptEvent interruptEvent;
  TLBOperationTarget target;
  TLBOperation operation;
} KTLBOperationsInterruptEvent;

/* 65 */
typedef struct KObjectName
{
  char name[12];
  KClientPort *parentClientPort;
} KObjectName;

/* 74 */
typedef enum MemState
{
  MEMSTATE_FREE = 0x0,
  MEMSTATE_RESERVED = 0x1,
  MEMSTATE_IO = 0x2,
  MEMSTATE_STATIC = 0x3,
  MEMSTATE_CODE = 0x4,
  MEMSTATE_PRIVATE = 0x5,
  MEMSTATE_SHARED = 0x6,
  MEMSTATE_CONTINUOUS = 0x7,
  MEMSTATE_ALIASED = 0x8,
  MEMSTATE_ALIAS = 0x9,
  MEMSTATE_ALIAS_CODE = 0xA,
  MEMSTATE_LOCKED = 0xB,
} MemState;

/* 75 */
typedef struct MemoryInfo
{
  void *baseVA;
  u32 size;
  u32 permissions;
  MemState state;
  u32 padding;
} MemoryInfo;

/* 76 */
typedef struct PageInfo
{
  u32 flags;
} PageInfo;

/* 78 */
typedef struct KTimerAndWDTManager
{
  KSynchronizationInterruptEvent syncInterruptEvent;
  u32 counter;
  u32 previousCounter;
  KTimeableInterruptEvent innerInterruptEvent;
  KRecursiveLock recursiveLock;
} KTimerAndWDTManager;

typedef enum ResetType
{
  RESET_ONESHOT = 0x0,
  RESET_STICKY = 0x1,
  RESET_PULSE = 0x2,
} ResetType;

/* 81 */
typedef struct PACKED ALIGN(4) KTimer
{
  KSynchronizationObject syncObject;
  KTimeableInterruptEvent timeableInterruptEvent;
  bool signaled;
  ResetType resetType;
  u16 padding;
  s64 interval;
  s64 currentValue;
  union KProcess *owner;
} KTimer;

/* 86 */
typedef struct KSchedulableInterruptEventLinkedList
{
  KSchedulableInterruptEvent *first;
  KSchedulableInterruptEvent *last;
  u32 unused;
  KThread *handlingKernelThread;
} KSchedulableInterruptEventLinkedList;

/* 87 */
typedef KSchedulableInterruptEvent KThreadTerminationInterruptEvent;

/* 88 */
typedef KSchedulableInterruptEvent KThreadExitInterruptEvent;

/* 89 */
typedef struct ALIGN(4) KInterruptEventMailbox
{
  u32 mailboxID;
  KSendableInterruptEvent *first;
  KSendableInterruptEvent *last;
  bool isBusy;
  KThread *handlingThread;
} KInterruptEventMailbox;

/* 94 */
typedef enum LimitableResource
{
  LIMITABLE_RESOURCE_MAX_PRIORITY = 0x0,
  LIMITABLE_RESOURCE_MAX_COMMIT = 0x1,
  LIMITABLE_RESOURCE_MAX_THREAD = 0x2,
  LIMITABLE_RESOURCE_MAX_EVENT = 0x3,
  LIMITABLE_RESOURCE_MAX_MUTEX = 0x4,
  LIMITABLE_RESOURCE_MAX_SEMAPHORE = 0x5,
  LIMITABLE_RESOURCE_MAX_TIMER = 0x6,
  LIMITABLE_RESOURCE_MAX_SHAREDMEMORY = 0x7,
  LIMITABLE_RESOURCE_MAX_ADDRESSARBITER = 0x8,
  LIMITABLE_RESOURCE_MAX_CPUTIME = 0x9,
  LIMITABLE_RESOURCE_END = 0xA,
  LIMITABLE_RESOURCE_MAX_BIT = 0x80000000,
} LimitableResource;

/* 99 */
typedef struct ALIGN(4) CpuRegisters
{
  u32 r[13];
  u32 sp;
  u32 lr;
  u32 pc;
  u32 cpsr;
} CpuRegisters;

/* 100 */
typedef struct FpuRegisters
{
  union
  {
      struct PACKED { double d[16]; };
      float  s[32];
  };
  u32 fpscr;
  u32 fpexc;
} FpuRegisters;

/* 98 */
typedef struct ThreadContext
{
  CpuRegisters cpuRegisters;
  FpuRegisters fpuRegisters;
} ThreadContext;

/* 101 */
typedef struct KAttachProcessDebugEventInfo
{
  union KProcess *process;
} KAttachProcessDebugEventInfo;

/* 102 */
typedef struct KAttachThreadDebugEventInfo
{
  u32 creatorThreadId;
  void *threadLocalStorage;
  u32 *processEntrypoint;
} KAttachThreadDebugEventInfo;


/// Reasons for an exit thread event.
typedef enum {
  EXITTHREAD_EVENT_EXIT              = 0, ///< Thread exited.
  EXITTHREAD_EVENT_TERMINATE         = 1, ///< Thread terminated.
  EXITTHREAD_EVENT_EXIT_PROCESS      = 2, ///< Process exited either normally or due to an uncaught exception.
  EXITTHREAD_EVENT_TERMINATE_PROCESS = 3, ///< Process has been terminated by @ref svcTerminateProcess.
} ExitThreadEventReason;

/// Reasons for an exit process event.
typedef enum {
  EXITPROCESS_EVENT_EXIT                = 0, ///< Process exited either normally or due to an uncaught exception.
  EXITPROCESS_EVENT_TERMINATE           = 1, ///< Process has been terminated by @ref svcTerminateProcess.
  EXITPROCESS_EVENT_DEBUG_TERMINATE     = 2, ///< Process has been terminated by @ref svcTerminateDebugProcess.
} ExitProcessEventReason;

/* 103 */
typedef struct KExitThreadDebugEventInfo
{
  ExitThreadEventReason reason;
} KExitThreadDebugEventInfo;

/* 104 */
typedef struct KExitProcessDebugEventInfo
{
  ExitProcessEventReason reason;
} KExitProcessDebugEventInfo;

/// Stop point types
typedef enum StopPointType
{
    STOPPOINT_SVC_FF        = 0, ///< See @ref SVC_STOP_POINT.
    STOPPOINT_BREAKPOINT    = 1, ///< Breakpoint.
    STOPPOINT_WATCHPOINT    = 2, ///< Watchpoint.
} StopPointType;

/* 105 */
typedef struct KFaultExceptionDebugEventInfo
{
  u32 faultInfo;
  StopPointType stopPointType;
} KFaultExceptionDebugEventInfo;

/// Reasons for a user break.
typedef enum UserBreakType
{
    USERBREAK_PANIC         = 0, ///< Panic.
    USERBREAK_ASSERT        = 1, ///< Assertion failed.
    USERBREAK_USER          = 2, ///< User related.
    USERBREAK_LOAD_RO       = 3, ///< Load RO.
    USERBREAK_UNLOAD_RO     = 4, ///< Unload RO.
} UserBreakType;

/* 106 */
typedef struct KUserBreakExceptionDebugEventInfo
{
  UserBreakType reason;
  u32 croInfo, croInfoSize;
} KUserBreakExceptionDebugEventInfo;

/* 107 */
typedef struct KDebuggerBreakDebugEventInfo
{
  s32 threadIds[4];
} KDebuggerBreakDebugEventInfo;

/* 109 */
typedef struct KExceptionDebugEventInfo
{
  ExceptionEventType type;
  void *address;
  u32 category;
  union
  {
    KFaultExceptionDebugEventInfo fault;
    KUserBreakExceptionDebugEventInfo userBreak;
    KDebuggerBreakDebugEventInfo debuggerBreak;
  };
} KExceptionDebugEventInfo;

/* 110 */
typedef struct KScheduleDebugEventInfo
{
  u64 clockTick;
  u32 cpuId;
  u32 unk[5];
  u32 unkEventInfo;
  u32 padding;
} KScheduleDebugEventInfo;

/* 111 */
typedef struct KSyscallDebugEventInfo
{
  u64 clockTick;
  u32 svcId;
} KSyscallDebugEventInfo;

/* 112 */
typedef struct KOutputStringDebugEventInfo
{
  const char *str;
  u32 size;
} KOutputStringDebugEventInfo;

/* 113 */
typedef struct KMapDebugEventInfo
{
  void *address;
  u32 size;
  MemPerm permissions;
  MemState state;
} KMapDebugEventInfo;

/* 115 */
typedef struct KEventInfo
{
  DebugEventType type;
  u32 threadId;
  u32 flags;
  bool processIsBeingAttached;
  bool needsToBeContinued;
  bool other;
  bool isProcessed;
  union
  {
    KAttachProcessDebugEventInfo attachProcess;
    KAttachThreadDebugEventInfo attachThread;
    KExitThreadDebugEventInfo exitThread;
    KExitProcessDebugEventInfo exitProcess;
    KExceptionDebugEventInfo exception;
    KScheduleDebugEventInfo schedule;
    KSyscallDebugEventInfo syscall;
    KOutputStringDebugEventInfo outputString;
    KMapDebugEventInfo map;
  };
} KEventInfo;

typedef struct ALIGN(0x1000) KCoreObjectContext
{
  KThread *volatile currentThread;
  union KProcess *volatile currentProcess;
  KScheduler *volatile currentScheduler;
  KSchedulableInterruptEventLinkedList *volatile currentSchedulableInterruptEventLinkedList;
  KThread *volatile lastKThreadEncounteringExceptionPtr;
  u8 padding[4];
  KThread schedulerThread;
  KScheduler schedulerInstance;
  KSchedulableInterruptEventLinkedList schedulableInterruptEventLinkedListInstance;
  u8 padding2[2164];
  u32 unkStack[291];
} KCoreObjectContext;

typedef struct KCoreContext
{
  u32 gap0[1024];
  u32 L1Table_exceptionStack[4096];
  u32 gap1[1024];
  u32 schedulerThreadContext[1024];
  u32 gap2[1024];
  KCoreObjectContext objectContext;
} KCoreContext;

static KCoreContext * const currentCoreContext = (KCoreContext *)0xFFFF1000;
extern KCoreContext *coreCtxs;

#define DEFINE_CONSOLE_SPECIFIC_STRUCTS(console, nbCores)
/* 60 */
typedef struct ALIGN(4) KProcessHwInfoN3DS
{
  KObjectMutex mutex;
  u32 processTLBEntriesNeedToBeFlushedOnCore[4];
  KLinkedList ownedKMemoryBlocks;
  u32 unknown_3;
  u32 field_28;
  u32 translationTableBase;
  u8 contextId;
  bool globalTLBFlushRequired;
  bool isCurrentlyLoadedApp;
  u32 unknown_5;
  void *endOfUserlandVmem;
  void *linearVAUserlandBase;
  u32 unknown_6;
  u32 mmuTableSize;
  u32 *mmuTableVA;
} KProcessHwInfoN3DS;

typedef struct ALIGN(4) KProcessHwInfoO3DS8x
{
  KObjectMutex mutex;
  u32 processTLBEntriesNeedToBeFlushedOnCore[2];
  KLinkedList ownedKMemoryBlocks;
  u32 unknown_3;
  u32 field_28;
  u32 translationTableBase;
  u8 contextId;
  bool globalTLBFlushRequired;
  bool isCurrentlyLoadedApp;
  u32 unknown_5;
  void *endOfUserlandVmem;
  void *linearVAUserlandBase;
  u32 unknown_6;
  u32 mmuTableSize;
  u32 *mmuTableVA;
} KProcessHwInfoO3DS8x;

typedef struct ALIGN(4) KProcessHwInfoO3DSPre8x
{
  KObjectMutex mutex;
  u32 processTLBEntriesNeedToBeFlushedOnCore[2];
  KLinkedList ownedKMemoryBlocks;
  u32 unknown_3;
  u32 field_28;
  u32 translationTableBase;
  u8 contextId;
  bool globalTLBFlushRequired;
  bool isCurrentlyLoadedApp;
  u32 unknown_5;
  void *endOfUserlandVmem;
  u32 mmuTableSize;
  u32 *mmuTableVA;
} KProcessHwInfoO3DSPre8x;

#define INSTANCIATE_KPROCESS(sys) \
typedef struct KProcess##sys\
{\
  KSynchronizationObject syncObject;\
  KSendableInterruptEvent sendableInterruptEvent;\
  KProcessHwInfo##sys hwInfo;\
  u32 totalThreadContextSize;\
  KLinkedList threadLocalPages;\
  u32 unknown_7;\
  s32 idealProcessor;\
  KDebug *debug;\
  KResourceLimit *resourceLimits;\
  ProcessStatus status;\
  u8 affinityMask;\
  u16 padding;\
  s16 threadCount;\
  s16 maxThreadCount;\
  u32 svcAccessControlMask[4];\
  u32 interruptFlags[4];\
  u32 kernelFlags;\
  u16 handleTableSize;\
  u16 kernelReleaseVersion;\
  KCodeSet *codeSet;\
  u32 processId;\
  s64 creationTime;\
  KThread *mainThread;\
  u32 interruptEnabledFlags[4];\
  KProcessHandleTable handleTable;\
  /* Custom fields for plugin system */ \
  /* { */ \
  u32     customFlags; /* see KProcess_CustomFlags enum below */ \
  Handle  onMemoryLayoutChangeEvent;\
  /* } */ \
  u8 gap234[44];\
  u64 unused;\
} KProcess##sys;

enum KProcess_CustomFlags
{
    ForceRWXPages = 1 << 0,
    SignalOnMemLayoutChanges = 1 << 1,
    SignalOnExit = 1 << 2,

    MemLayoutChanged = 1 << 16
};

INSTANCIATE_KPROCESS(N3DS);
INSTANCIATE_KPROCESS(O3DS8x);
INSTANCIATE_KPROCESS(O3DSPre8x);

typedef union KProcessHwInfo
{
    KProcessHwInfoN3DS N3DS;
    KProcessHwInfoO3DS8x O3DS8x;
    KProcessHwInfoO3DSPre8x O3DSPre8x;
} KProcessHwInfo;

typedef union KProcess
{
    KProcessN3DS N3DS;
    KProcessO3DS8x O3DS8x;
    KProcessO3DSPre8x O3DSPre8x;
} KProcess;

/* 54 */
typedef struct InterruptManagerN3DS
{
  InterruptData privateInterrupts[4][32];
  InterruptData publicEvents[96];
  KObjectMutex mutex;
} InterruptManagerN3DS;

typedef struct InterruptManagerO3DS
{
  InterruptData privateInterrupts[2][32];
  InterruptData publicEvents[96];
  KObjectMutex mutex;
} InterruptManagerO3DS;

typedef union InterruptManager
{
    InterruptManagerN3DS N3DS;
    InterruptManagerO3DS O3DS;
} InterruptManager;

typedef struct AddressRange
{
  void *begin;
  void *end;
} AddressRange;

typedef struct KAsyncCacheMaintenanceInterruptEvent
{
  KSchedulableInterruptEvent schedulableInterruptEvent;
  const KThread *handlingThread;
} KAsyncCacheMaintenanceInterruptEvent;

typedef struct KCacheMaintenanceInterruptEventN3DS
{
  KBaseInterruptEvent baseInterruptEvent;
  u8 cacheMaintenanceOperation;
  bool async;
  s8 remaining;
  KThread *operatingThread;
  AddressRange addrRange;
  KThreadLinkedListNode *threadListNode;
  KThreadLinkedList *threadList;
  KAsyncCacheMaintenanceInterruptEvent asyncInterruptEventsPerCore[4];
} KCacheMaintenanceInterruptEventN3DS;

typedef struct KCacheMaintenanceInterruptEventO3DS
{
  KBaseInterruptEvent baseInterruptEvent;
  u8 cacheMaintenanceOperation;
  bool async;
  s8 remaining;
  KThread *operatingThread;
  AddressRange addrRange;
  KThreadLinkedListNode *threadListNode;
  KThreadLinkedList *threadList;
  KAsyncCacheMaintenanceInterruptEvent asyncInterruptEventsPerCore[2];
} KCacheMaintenanceInterruptEventO3DS;

typedef union KCacheMaintenanceInterruptEvent
{
    KCacheMaintenanceInterruptEventN3DS N3DS;
    KCacheMaintenanceInterruptEventO3DS O3DS;
} KCacheMaintenanceInterruptEvent;

typedef struct FcramLayout
{
  u32 applicationAddr;
  u32 applicationSize;
  u32 systemAddr;
  u32 systemSize;
  u32 baseAddr;
  u32 baseSize;
} FcramLayout;

typedef struct RegionDescriptor
{
    void               *firstMemoryBlock;
    void               *lastMemoryBlock;
    void               *regionStart;
    u32                 regionSizeInBytes;
}   RegionDescriptor;

typedef struct FcramDescriptor
{
    RegionDescriptor    appRegion;
    RegionDescriptor    sysRegion;
    RegionDescriptor    baseRegion;
    RegionDescriptor *  regionDescsPtr;
    u32                 fcramStart;
    u32                 fcramSizeInPages;
    u32                 baseMemoryStart;
    u32                 kernelUsageInBytes;
    u32                 unknown;
    KObjectMutex        mutex;
}   FcramDescriptor;

extern bool isN3DS;
extern void *officialSVCs[0x7E];

#define KPROCESSRELATED_OFFSETOFF(classname, field) (isN3DS ? offsetof(classname##N3DS, field) :\
((GET_VERSION_MINOR(kernelVersion) >= 44) ? offsetof(classname##O3DS8x, field) :\
offsetof(classname##O3DSPre8x, field)))

#define KPROCESSRELATED_GET_PTR(obj, field) (isN3DS ? &(obj)->N3DS.field :\
((GET_VERSION_MINOR(kernelVersion) >= 44) ? &(obj)->O3DS8x.field :\
&(obj)->O3DSPre8x.field))

#define KPROCESSRELATED_GET_PTR_TYPE(type, obj, field) (isN3DS ? (type *)(&(obj)->N3DS.field) :\
((GET_VERSION_MINOR(kernelVersion) >= 44) ? (type *)(&(obj)->O3DS8x.field) :\
(type *)(&(obj)->O3DSPre8x.field)))

#define KPROCESS_OFFSETOF(field)                    KPROCESSRELATED_OFFSETOFF(KProcess, field)
#define KPROCESS_GET_PTR(obj, field)                KPROCESSRELATED_GET_PTR(obj, field)
#define KPROCESS_GET_PTR_TYPE(type, obj, field)     KPROCESSRELATED_GET_PTR_TYPE(type, obj, field)
#define KPROCESS_GET_RVALUE(obj, field)             *(KPROCESS_GET_PTR(obj, field))
#define KPROCESS_GET_RVALUE_TYPE(type, obj, field)  *(KPROCESS_GET_PTR_TYPE(type, obj, field))

#define KPROCESSHWINFO_OFFSETOF(field)                    KPROCESSRELATED_OFFSETOFF(KProcessHwInfo, field)
#define KPROCESSHWINFO_GET_PTR(obj, field)                KPROCESSRELATED_GET_PTR(obj, field)
#define KPROCESSHWINFO_GET_PTR_TYPE(type, obj, field)     KPROCESSRELATED_GET_PTR_TYPE(type, obj, field)
#define KPROCESSHWINFO_GET_RVALUE(obj, field)             *(KPROCESSHWINFO_GET_PTR(obj, field))
#define KPROCESSHWINFO_GET_RVALUE_TYPE(type, obj, field)  *(KPROCESSHWINFO_GET_PTR_TYPE(type, obj, field))

extern u32 pidOffsetKProcess, hwInfoOffsetKProcess, codeSetOffsetKProcess, handleTableOffsetKProcess, debugOffsetKProcess;

static inline u32 idOfProcess(KProcess *process)
{
    u32 id;
    memcpy(&id, (const u8 *)process + pidOffsetKProcess, 4);
    return id;
}

static inline KProcessHwInfo *hwInfoOfProcess(KProcess *process)
{
    return (KProcessHwInfo *)((uintptr_t)process + hwInfoOffsetKProcess);
}

static inline KCodeSet *codeSetOfProcess(KProcess *process)
{
    KCodeSet *cs;
    memcpy(&cs, (const u8 *)process + codeSetOffsetKProcess, 4);
    return cs;
}

static inline KProcessHandleTable *handleTableOfProcess(KProcess *process)
{
    return (KProcessHandleTable *)((uintptr_t)process + handleTableOffsetKProcess);
}

static inline KDebug *debugOfProcess(KProcess *process)
{
    KDebug *debug;
    memcpy(&debug, (const u8 *)process + debugOffsetKProcess, 4);
    return debug;
}

static inline const char *classNameOfAutoObject(KAutoObject *object)
{
    const char *name;
    if(GET_VERSION_MINOR(kernelVersion) >= 46)
    {
        KClassToken tok;
        object->vtable->GetClassToken(&tok, object);
        name = tok.name;
    }
    else
        name = object->vtable->GetClassName(object);
    return name;
}

extern Result (*KProcessHandleTable__CreateHandle)(KProcessHandleTable *this, Handle *out, KAutoObject *obj, u8 token);

static inline Result createHandleForProcess(Handle *out, KProcess *process, KAutoObject *obj)
{
    u8 token;
    if(GET_VERSION_MINOR(kernelVersion) >= 46)
    {
        KClassToken tok;
        obj->vtable->GetClassToken(&tok, obj);
        token = tok.flags;
    }
    else
        token = (u8)(u32)(obj->vtable->GetClassName(obj));

    KProcessHandleTable *handleTable = handleTableOfProcess(process);
    return KProcessHandleTable__CreateHandle(handleTable, out, obj, token);
}

static inline Result createHandleForThisProcess(Handle *out, KAutoObject *obj)
{
    return createHandleForProcess(out, currentCoreContext->objectContext.currentProcess, obj);
}
