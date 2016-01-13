#pragma once

#ifdef KHAX_DEBUG
	#define KHAX_printf(...) printf(__VA_ARGS__), gspWaitForVBlank(), gfxFlushBuffers(), gfxSwapBuffers()
#else
	#define KHAX_printf(...) gspWaitForVBlank(), gfxFlushBuffers(), gfxSwapBuffers()
#endif

// Shut up IntelliSense warnings when using MSVC as an IDE, even though MSVC will obviously never
// actually compile this program.
#ifdef _MSC_VER
	#undef ALIGN
	#define ALIGN(x) __declspec(align(x))
	#if _MSC_VER < 1900
		#define alignof __alignof
	#endif
	#define KHAX_ATTRIBUTE(...)
#else
	#define KHAX_ATTRIBUTE(...) __VA_ARGS__
#endif

#define KHAX_lengthof(...) (sizeof(__VA_ARGS__) / sizeof((__VA_ARGS__)[0]))
#define KHAX_UNUSED(...) static_cast<void>(__VA_ARGS__)

//------------------------------------------------------------------------------------------------
namespace KHAX
{
	//------------------------------------------------------------------------------------------------
	// This code uses offsetof illegally (i.e. on polymorphic classes).
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Winvalid-offsetof"

	//------------------------------------------------------------------------------------------------
	// General linked list node kernel object.
	struct KLinkedListNode
	{
		KLinkedListNode *next;
		KLinkedListNode *prev;
		void *data;
	};
	static_assert(sizeof(KLinkedListNode) == 0x00C, "KLinkedListNode isn't the expected size.");

	//------------------------------------------------------------------------------------------------
	// Base class of reference-counted kernel objects.
	class KAutoObject
	{
	public:
		u32 m_refCount;                                 // +004

	protected:
		virtual ~KAutoObject() {}
	};
	static_assert(sizeof(KAutoObject) == 0x008, "KAutoObject isn't the expected size.");
	static_assert(offsetof(KAutoObject, m_refCount) == 0x004, "KAutoObject isn't the expected layout.");

	//------------------------------------------------------------------------------------------------
	// Base class of synchronizable objects.
	class KSynchronizationObject : public KAutoObject
	{
	public:
		u32 m_threadSyncCount;                          // +008
		KLinkedListNode *m_threadSyncFirst;             // +00C
		KLinkedListNode *m_threadSyncLast;              // +010
	};
	static_assert(sizeof(KSynchronizationObject) == 0x014, "KSynchronizationObject isn't the expected size.");
	static_assert(offsetof(KSynchronizationObject, m_threadSyncCount) == 0x008,
		"KSynchronizationObject isn't the expected layout.");

	//------------------------------------------------------------------------------------------------
	struct KDebugThread;
	struct KThreadLocalPage;
	class KCodeSet;

	//------------------------------------------------------------------------------------------------
	// Unofficial name
	typedef u8 KSVCACL[0x80 / 8];

	//------------------------------------------------------------------------------------------------
	// ARM VFP register
	union KHAX_ATTRIBUTE(__attribute__((__aligned__(4))) __attribute__((__packed__))) VFPRegister
	{
		float m_single[2];
		double m_double;
	};
	static_assert(alignof(VFPRegister) == 0x004,
		"VFPRegister isn't the expected alignment.");
	static_assert(sizeof(VFPRegister) == 0x008,
		"VFPRegister isn't the expected size.");

	//------------------------------------------------------------------------------------------------
	// SVC-mode register save area.
	// http://3dbrew.org/wiki/Memory_layout#0xFF4XX000
	struct SVCRegisterState
	{
		u32 m_r4;                                       // +000
		u32 m_r5;                                       // +004
		u32 m_r6;                                       // +008
		u32 m_r7;                                       // +00C
		u32 m_r8;                                       // +010
		u32 m_r9;                                       // +014
		u32 m_sl;                                       // +018
		u32 m_fp;                                       // +01C
		u32 m_sp;                                       // +020
		u32 m_lr;                                       // +024
	};
	static_assert(sizeof(SVCRegisterState) == 0x028,
		"SVCRegisterState isn't the expected size.");

	//------------------------------------------------------------------------------------------------
	// SVC-mode thread state structure.  This is the last part of the per-
	// thread page allocated in 0xFF4XX000.
	// http://3dbrew.org/wiki/Memory_layout#0xFF4XX000
	struct SVCThreadArea
	{
		KSVCACL m_svcAccessControl;                     // +000
		u32 m_unknown010;                               // +010
		u32 m_unknown014;                               // +014
		SVCRegisterState m_svcRegisterState;            // +018
		VFPRegister m_vfpRegisters[16];                 // +040
		u32 m_unknown0C4;                               // +0C0
		u32 m_fpexc;                                    // +0C4
	};
	static_assert(offsetof(SVCThreadArea, m_svcRegisterState) == 0x018,
		"ThreadSVCArea isn't the expected layout.");
	static_assert(sizeof(SVCThreadArea) == 0x0C8,
		"ThreadSVCArea isn't the expected size.");

	//------------------------------------------------------------------------------------------------
	// Kernel's internal structure of a thread object.
	class KThread : public KSynchronizationObject
	{
	public:
		u32 m_unknown014;                               // +014
		u32 m_unknown018;                               // +018
		u32 m_unknown01C;                               // +01C
		u32 m_unknown020;                               // +020
		u32 m_unknown024;                               // +024
		u32 m_unknown028;                               // +028
		u32 m_unknown02C;                               // +02C
		u32 m_unknown030;                               // +030
		u32 m_unknown034;                               // +034
		KDebugThread *m_debugThread;                    // +038
		s32 m_threadPriority;                           // +03C
		void *m_waitingOnObject;                        // +040
		u32 m_unknown044;                               // +044
		KThread **m_schedulerUnknown048;                // +048
		void *m_arbitrationAddress;                     // +04C
		u32 m_unknown050;                               // +050
		u32 m_unknown054;                               // +054
		u32 m_unknown058;                               // +058
		KLinkedListNode *m_waitingOnList;               // +05C
		u32 m_unknownListCount;                         // +060
		KLinkedListNode *m_unknownListHead;             // +064
		KLinkedListNode *m_unknownListTail;             // +068
		s32 m_threadPriority2;                          // +06C
		s32 m_creatingProcessor;                        // +070
		u32 m_unknown074;                               // +074
		u32 m_unknown078;                               // +078
		u16 m_unknown07C;                               // +07C
		u8 m_threadType;                                // +07E
		u8 m_padding07F;                                // +07F
		void *m_process;                                // +080
		u32 m_threadID;                                 // +084
		SVCRegisterState *m_svcRegisterState;           // +088
		void *m_svcPageEnd;                             // +08C
		s32 m_idealProcessor;                           // +090
		void *m_tlsUserMode;                            // +094
		void *m_tlsKernelMode;                          // +098
		u32 m_unknown09C;                               // +09C
		KThread *m_prev;                                // +0A0
		KThread *m_next;                                // +0A4
		KThread **m_temporaryLinkedList;                // +0A8
		u32 m_unknown0AC;                               // +0B0
	};
	static_assert(sizeof(KThread) == 0x0B0,
		"KThread isn't the expected size.");
	static_assert(offsetof(KThread, m_svcRegisterState) == 0x088,
		"KThread isn't the expected layout.");

	//------------------------------------------------------------------------------------------------
	// Kernel's internal structure of a process object.
	// Version 1.0.0(?) - 7.2.0
	class KProcess_1_0_0_Old : public KSynchronizationObject
	{
	public:
		u32 m_unknown014;                               // +014
		u32 m_unknown018;                               // +018
		KThread *volatile m_interactingThread;          // +01C
		u16 m_unknown020;                               // +020
		u16 m_unknown022;                               // +022
		u32 m_unknown024;                               // +024
		u32 m_unknown028;                               // +028
		u32 m_memoryBlockCount;                         // +02C
		KLinkedListNode *m_memoryBlockFirst;            // +030
		KLinkedListNode *m_memoryBlockLast;             // +034
		u32 m_unknown038;                               // +038
		u32 m_unknown03C;                               // +03C
		void *m_translationTableBase;                   // +040
		u8 m_contextID;                                 // +044
		u32 m_unknown048;                               // +048
		u32 m_unknown04C;                               // +04C
		u32 m_mmuTableSize;                             // +050
		void *m_mmuTableAddress;                        // +054
		u32 m_threadContextPagesSize;                   // +058
		u32 m_threadLocalPageCount;                     // +05C
		KLinkedListNode *m_threadLocalPageFirst;        // +060
		KLinkedListNode *m_threadLocalPageLast;         // +064
		u32 m_unknown068;                               // +068
		s32 m_idealProcessor;                           // +06C
		u32 m_unknown070;                               // +070
		void *m_resourceLimits;                         // +074
		u8 m_unknown078;                                // +078
		u8 m_affinityMask;                              // +079
		u32 m_threadCount;                              // +07C
		KSVCACL m_svcAccessControl;                     // +080
		u32 m_interruptFlags[0x80 / 32];                // +090
		u32 m_kernelFlags;                              // +0A0
		u16 m_handleTableSize;                          // +0A4
		u16 m_kernelReleaseVersion;                     // +0A6
		KCodeSet *m_codeSet;                            // +0A8
		u32 m_processID;                                // +0AC
		u32 m_kernelFlags2;                             // +0B0
		u32 m_unknown0B4;                               // +0B4
		KThread *m_mainThread;                          // +0B8
		//...more...
	};
	static_assert(offsetof(KProcess_1_0_0_Old, m_svcAccessControl) == 0x080,
		"KProcess_1_0_0_Old isn't the expected layout.");

	//------------------------------------------------------------------------------------------------
	// Kernel's internal structure of a process object.
	// Old 3DS Version 8.0.0 - 9.5.0...
	class KProcess_8_0_0_Old : public KSynchronizationObject
	{
	public:
		u32 m_unknown014;                               // +014
		u32 m_unknown018;                               // +018
		KThread *volatile m_interactingThread;          // +01C
		u16 m_unknown020;                               // +020
		u16 m_unknown022;                               // +022
		u32 m_unknown024;                               // +024
		u32 m_unknown028;                               // +028
		u32 m_memoryBlockCount;                         // +02C
		KLinkedListNode *m_memoryBlockFirst;            // +030
		KLinkedListNode *m_memoryBlockLast;             // +034
		u32 m_unknown038;                               // +038
		u32 m_unknown03C;                               // +03C
		void *m_translationTableBase;                   // +040
		u8 m_contextID;                                 // +044
		u32 m_unknown048;                               // +048
		void *m_userVirtualMemoryEnd;                   // +04C
		void *m_userLinearVirtualBase;                  // +050
		u32 m_unknown054;                               // +054
		u32 m_mmuTableSize;                             // +058
		void *m_mmuTableAddress;                        // +05C
		u32 m_threadContextPagesSize;                   // +060
		u32 m_threadLocalPageCount;                     // +064
		KLinkedListNode *m_threadLocalPageFirst;        // +068
		KLinkedListNode *m_threadLocalPageLast;         // +06C
		u32 m_unknown070;                               // +070
		s32 m_idealProcessor;                           // +074
		u32 m_unknown078;                               // +078
		void *m_resourceLimits;                         // +07C
		u32 m_unknown080;                               // +080
		u32 m_threadCount;                              // +084
		u8 m_svcAccessControl[0x80 / 8];                // +088
		u32 m_interruptFlags[0x80 / 32];                // +098
		u32 m_kernelFlags;                              // +0A8
		u16 m_handleTableSize;                          // +0AC
		u16 m_kernelReleaseVersion;                     // +0AE
		KCodeSet *m_codeSet;                            // +0B0
		u32 m_processID;                                // +0B4
		u32 m_unknown0B8;                               // +0B8
		u32 m_unknown0BC;                               // +0BC
		KThread *m_mainThread;                          // +0C0
		//...more...
	};
	static_assert(offsetof(KProcess_8_0_0_Old, m_svcAccessControl) == 0x088,
		"KProcess_8_0_0_Old isn't the expected layout.");

	//------------------------------------------------------------------------------------------------
	// Kernel's internal structure of a process object.
	// New 3DS Version 8.0.0 - 9.5.0...
	class KProcess_8_0_0_New : public KSynchronizationObject
	{
	public:
		u32 m_unknown014;                               // +014
		u32 m_unknown018;                               // +018
		KThread *volatile m_interactingThread;          // +01C
		u16 m_unknown020;                               // +020
		u16 m_unknown022;                               // +022
		u32 m_unknown024;                               // +024
		u32 m_unknown028;                               // +028
		u32 m_unknown02C;                               // +02C new to New 3DS
		u32 m_unknown030;                               // +030 new to New 3DS
		u32 m_memoryBlockCount;                         // +034
		KLinkedListNode *m_memoryBlockFirst;            // +038
		KLinkedListNode *m_memoryBlockLast;             // +03C
		u32 m_unknown040;                               // +040
		u32 m_unknown044;                               // +044
		void *m_translationTableBase;                   // +048
		u8 m_contextID;                                 // +04C
		u32 m_unknown050;                               // +050
		void *m_userVirtualMemoryEnd;                   // +054
		void *m_userLinearVirtualBase;                  // +058
		u32 m_unknown05C;                               // +05C
		u32 m_mmuTableSize;                             // +060
		void *m_mmuTableAddress;                        // +064
		u32 m_threadContextPagesSize;                   // +068
		u32 m_threadLocalPageCount;                     // +06C
		KLinkedListNode *m_threadLocalPageFirst;        // +070
		KLinkedListNode *m_threadLocalPageLast;         // +074
		u32 m_unknown078;                               // +078
		s32 m_idealProcessor;                           // +07C
		u32 m_unknown080;                               // +080
		void *m_resourceLimits;                         // +084
		u32 m_unknown088;                               // +088
		u32 m_threadCount;                              // +08C
		u8 m_svcAccessControl[0x80 / 8];                // +090
		u32 m_interruptFlags[0x80 / 32];                // +0A0
		u32 m_kernelFlags;                              // +0B0
		u16 m_handleTableSize;                          // +0B4
		u16 m_kernelReleaseVersion;                     // +0B6
		KCodeSet *m_codeSet;                            // +0B8
		u32 m_processID;                                // +0BC
		u32 m_unknown0C0;                               // +0C0
		u32 m_unknown0C4;                               // +0C4
		KThread *m_mainThread;                          // +0C8
		//...more...
	};
	static_assert(offsetof(KProcess_8_0_0_New, m_svcAccessControl) == 0x090,
		"KProcess_8_0_0_New isn't the expected layout.");

	//------------------------------------------------------------------------------------------------
	// Done using illegal offsetof
	#pragma GCC diagnostic pop
}
