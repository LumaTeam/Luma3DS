#include <3ds.h>
#include "reslimit.h"
#include "util.h"
#include "manager.h"
#include "luma.h"

#define CPUTIME_MULTI_MASK      BIT(7)
#define CPUTIME_SINGLE_MASK     0

typedef s64 ReslimitValues[10];

static const ResourceLimitType g_reslimitInitOrder[10] = {
    RESLIMIT_COMMIT,
    RESLIMIT_PRIORITY,
    RESLIMIT_THREAD,
    RESLIMIT_EVENT,
    RESLIMIT_MUTEX,
    RESLIMIT_SEMAPHORE,
    RESLIMIT_TIMER,
    RESLIMIT_SHAREDMEMORY,
    RESLIMIT_ADDRESSARBITER,
    RESLIMIT_CPUTIME,
};

static u32 g_currentAppMemLimit = 0, g_defaultAppMemLimit;

static ReslimitValues g_o3dsReslimitValues[4] = {
    // APPLICATION
    {
        0x4000000,  // Allocatable memory (dynamically calculated)
        0x18,       // Max. priority
        32,         // Threads
        32,         // Events
        32,         // Mutexes
        8,          // Semaphores
        8,          // Timers
        16,         // Shared memory objects
        2,          // Address arbiters
        0,          // Core1 CPU time
    },

    // SYS_APPLET
    {
        0x2606000,  // Allocatable memory (dynamically calculated)
        4,          // Max priority
        14,         // Threads
        8,          // Events
        8,          // Mutexes
        4,          // Semaphores
        4,          // Timers
        8,          // Shared memory objects
        3,          // Address arbiters
        10000,      // Core1 CPU time
    },

    // LIB_APPLET
    {
        0x0602000,  // Allocatable memory (dynamically calculated)
        4,          // Max priority
        14,         // Threads
        8,          // Events
        8,          // Mutexes
        4,          // Semaphores
        4,          // Timers
        8,          // Shared memory objects
        1,          // Address arbiters
        10000,      // Core1 CPU time
    },

    // OTHER (BASE sysmodules)
    {
        0x1682000,  // Allocatable memory (dynamically calculated)
        4,          // Max priority
        202,        // Threads
        248,        // Events
        35,         // Mutexes
        64,         // Semaphores
        43,         // Timers
        30,         // Shared memory objects
        43,         // Address arbiters
        1000,       // Core1 CPU time
    },
};

static ReslimitValues g_n3dsReslimitValues[4] = {
    // APPLICATION
    {
        0x7C00000,  // Allocatable memory (dynamically calculated)
        0x18,       // Max. priority
        32,         // Threads
        32,         // Events
        32,         // Mutexes
        8,          // Semaphores
        8,          // Timers
        16,         // Shared memory objects
        2,          // Address arbiters
        0,          // Core1 CPU time
    },

    // SYS_APPLET
    {
        0x5E06000,  // Allocatable memory (dynamically calculated)
        4,          // Max priority
        29,         // Threads
        11,         // Events
        8,          // Mutexes
        4,          // Semaphores
        4,          // Timers
        8,          // Shared memory objects
        3,          // Address arbiters
        10000,      // Core1 CPU time
    },

    // LIB_APPLET
    {
        0x0602000,  // Allocatable memory (dynamically calculated)
        4,          // Max priority
        14,         // Threads
        8,          // Events
        8,          // Mutexes
        4,          // Semaphores
        4,          // Timers
        8,          // Shared memory objects
        1,          // Address arbiters
        10000,      // Core1 CPU time
    },

    // OTHER (BASE sysmodules)
    {
        0x2182000,  // Allocatable memory (dynamically calculated)
        4,          // Max priority
        225,        // Threads
        264,        // Events
        37,         // Mutexes
        67,         // Semaphores
        44,         // Timers
        31,         // Shared memory objects
        45,         // Address arbiters
        1000,       // Core1 CPU time
    },
};

/*
    SCHEDULER AND PREEMPTION STUFF ON THE 3DS:

    Most of the basic scheduler stuff is the same between the Switch and the 3DS:
        - Multi-level queue with 64 levels, 0 being highest priority.
        - Interrupt handling code mostly the same, same thing for the entire irq bottom-half
        object hierarchy.
        - There is the same global critical section object, although not fully a spinlock,
        and critsec Leave code has the same logic w/r/t the scheduler and scheduler interrupts.

    Some stuff is different:
        - The thread selection is done core-per-core, and not all-at-once like on the Switch.
        - No load balancing. SetThreadAffinityMask is not implemented either.
            - When needed (thread creation), each scheduler has a thread queue;
            other schedulers enqueue threads on these queues, triggering the scheduler
            SGI when transferring a thread, to make sure threads run on the right core.
        - Preemption stuff is different. Preemption is only on core1 when enabled.
        - Only one time source, on core1.
        - On 3DS, the process affinity mask (specified in exheader) IS NOT A LIMITER.
        ThreadAffinityMask = processAffinityMask | (1 << coreId). processAffinityMask is only used
        for cpuId < 0 in CreateThread.

    3DS CPUTIME

    Seemingly implemented since 2.1 (see pm workaround and kernel workaround for 1.0 titles below),
    on 1.0, there was not any restriction on core1 usage (?).

    There are 2 preemption modes, controlled by svcKernelSetState(6, 3, (u64)x) (switching fails
    if any of those are still active).

    Both rely on segregating running thread/processes in 3 categories: "application", "sysmodules"
    and "exempted" (applets).

    Cputime values have different meanings:
        - 0: disables preemption. Prevent applications from spawning threads in core1.
        Behavior is undefined if an 1..89 reslimit is still running. Intended to be ran before app creation only.
        - 1..89: core1 cputime percentage. Enables preemption. Previous threads from the same
        reslimit are not affected (bug?), but *all* threads with prio >= 16 from the "sysmodule" category
        (reslimit value 1000, see below) are affected and possibly immediately setup for preemption, depending on their
        current reslimit value.
        - 1000: Sets the "sysmodule" preemption info to point to the current reslimit's,
        for when something else (application cputime reslimit being set by PM) is set to 1..89.
        - 10000 and above: exempted from preemption.
        - other values: buggy/possibly undefined?

    For the following, time durations might be off by a factor of 2 (?), and nothing was tested.
    Please check on hardware/emu code. Really really needs proofreading/prooftesting, especially mode0.
    The timers are updated on scheduler thread reselection.

    Both modes pause threads they don't want to run in thread selection, and unpause them when needed.
    If the threads that are intended to be paused is running an SVC, the pause will happen *after* SVC return.

    Mode0 "multi"

    Starting by "sysmodule" threads, alternatively allow (if preemptible) only sysmodule threads,
    and then only application threads to run.
    The latter has an exception; if "sysmodule" threads have run for less than 8usec (value is a kernel bug), they
    are unpaused an allowed to run instead.

    This happens at a rate of 2ms * (cpuTime/100).


    Mode1 "single"

    This mode is half-broken due to a kernel bug (when "current thread" is the priority 0 kernel thread).

    When this mode is enabled, only one application thread is allowed to be created on core1.

    This divides the core1 time into slices of 25ms.

    The "application" thread is given cpuTime% of the slice.
    The "sysmodules" threads are given a total of (90 - cpuTime)% of the slice.
    10% remain for other threads.
*/

static const struct {
    u32 titleUid;
    u32 value;
} g_startCpuTimeOverrides[] = {
    /*
        Region-incoherent? CHN/KOR/TWN consistently missing.
        This seems to be here to replace a 2.0+ kernel workaround for 1.0 titles (maybe?).

        It seems like 1.0 kernel didn't implement cputime reslimit & you could freely access core1.
        2.1+ kernel returns an error if you try to start an application thread on core1 with cputime=0,
        except for 1.0 titles for which it automatically sets cputime to 25.
    */
    { 0x205, 10000 }, // 3DS sound (JPN)
    { 0x215, 10000 }, // 3DS sound (USA)
    { 0x225, 10000 }, // 3DS sound (EUR)
    { 0x304, 10000 }, // Star Fox 64 3D (JPN)
    { 0x32E, 10000 }, // Super Monkey Ball 3D (JPN)
    { 0x334, 30    }, // Zelda no Densetsu: Toki no Ocarina 3D (JPN)
    { 0x335, 30    }, // The Legend of Zelda: Ocarina of Time 3D (USA)
    { 0x336, 30    }, // The Legend of Zelda: Ocarina of Time 3D (EUR)
    { 0x348, 10000 }, // Doctor Lautrec to Boukyaku no Kishidan (JPN)
    { 0x349, 10000 }, // Pro Yakyuu Spirits 2011 (JPN)
    { 0x368, 10000 }, // Doctor Lautrec and the Forgotten Knights (USA)
    { 0x370, 10000 }, // Super Monkey Ball 3D (USA)
    { 0x389, 10000 }, // Super Monkey Ball 3D (EUR)
    { 0x490, 10000 }, // Star Fox 64 3D (USA)
    { 0x491, 10000 }, // Star Fox 64 3D (EUR)
    { 0x562, 10000 }, // Doctor Lautrec and the Forgotten Knights (EUR)
};

static ReslimitValues *fixupReslimitValues(void)
{
    // In order: APPLICATION, SYS_APPLET, LIB_APPLET, OTHER
    // Fixup "commit" reslimit

    // Note: we lie in the reslimit and make as if neither KExt nor Roslina existed, to avoid breakage

    u32 appmemalloc = OS_KernelConfig->memregion_sz[0];
    u32 sysmemalloc = OS_KernelConfig->memregion_sz[1] + (hasKExt() ? getStolenSystemMemRegionSize() : 0);
    ReslimitValues *values = !IS_N3DS ? g_o3dsReslimitValues : g_n3dsReslimitValues;

    static const u32 minAppletMemAmount = 0x1200000;
    u32 defaultMemAmount = !IS_N3DS ? 0x2C00000 : 0x6400000;
    u32 otherMinOvercommitAmount = !IS_N3DS ? 0x280000 : 0x180000;
    u32 baseRegionSize = !IS_N3DS ? 0x1400000 : 0x2000000;

    if (sysmemalloc < minAppletMemAmount) {
        values[1][0] = sysmemalloc - minAppletMemAmount / 3;
        values[2][0] = 0;
        values[3][0] = baseRegionSize + otherMinOvercommitAmount;
    } else {
        u32 excess = sysmemalloc < defaultMemAmount ? 0 : sysmemalloc - defaultMemAmount;
        values[1][0] = 3 * excess / 4 + sysmemalloc - minAppletMemAmount / 3;
        values[2][0] = 1 * excess / 4 + minAppletMemAmount / 3;
        values[3][0] = baseRegionSize + (otherMinOvercommitAmount + excess / 4);
    }

    values[0][0] = appmemalloc;
    g_defaultAppMemLimit = appmemalloc;

    return values;
}

Result initializeReslimits(void)
{
    Result res = 0;
    ReslimitValues *values = fixupReslimitValues();
    for (u32 i = 0; i < 4; i++) {
        TRY(svcCreateResourceLimit(&g_manager.reslimits[i]));
        TRY(svcSetResourceLimitValues(g_manager.reslimits[i], g_reslimitInitOrder, values[i], 10));
    }

    return res;
}

Result setAppMemLimit(u32 limit)
{
    if (g_currentAppMemLimit == g_defaultAppMemLimit) {
        return 0;
    }

    ResourceLimitType category = RESLIMIT_COMMIT;
    s64 value = limit | ((u64)limit << 32); // high u32 is the value written to APPMEMALLOC
    return svcSetResourceLimitValues(g_manager.reslimits[0], &category, &value, 1);
}

Result resetAppMemLimit(void)
{
    return setAppMemLimit(g_defaultAppMemLimit);
}

Result setAppCpuTimeLimit(s64 limit)
{
    ResourceLimitType category = RESLIMIT_CPUTIME;
    return svcSetResourceLimitValues(g_manager.reslimits[0], &category, &limit, 1);
}

static Result getAppCpuTimeLimit(s64 *limit)
{
    ResourceLimitType category = RESLIMIT_CPUTIME;
    return svcGetResourceLimitLimitValues(limit, g_manager.reslimits[0], &category, 1);
}

void setAppCpuTimeLimitAndSchedModeFromDescriptor(u64 titleId, u16 descriptor)
{
    /*
        Two main cases here:
            - app has a non-0 cputime descriptor in exhdr: maximum core1 cputime reslimit and scheduling
            mode are set according to it. Current reslimit is set to 0. SetAppResourceLimit *is* needed
            to use core1.
            - app has a 0 cputime descriptor: maximum is set to 80, scheduling mode to "single" (broken).
            Current reslimit is set to 0, and SetAppResourceLimit *is* also needed
            to use core1, **EXCEPT** for an hardcoded set of titles.
    */
    u8 cpuTime = (u8)descriptor;
    assertSuccess(setAppCpuTimeLimit(0)); // remove preemption first.

    g_manager.cpuTimeBase = 0;
    u32 currentValueToSet = g_manager.cpuTimeBase; // 0

    if (cpuTime == 0) {
        // 2.0 apps have this exheader field correctly filled, very often to 0x9E (1.0 titles don't).
        u32 titleUid = ((u32)titleId >> 8) & 0xFFFFF;

        // Default setting is 80% max "single", with a current value of 0
        cpuTime = CPUTIME_SINGLE_MASK | 80;

        static const u32 numOverrides = sizeof(g_startCpuTimeOverrides) / sizeof(g_startCpuTimeOverrides[0]);

        if (titleUid >= g_startCpuTimeOverrides[0].titleUid && titleUid <= g_startCpuTimeOverrides[numOverrides - 1].titleUid) {
            u32 i = 0;
            for (u32 i = 0; i < numOverrides && titleUid < g_startCpuTimeOverrides[i].titleUid; i++);
            if (i < numOverrides) {
                if (g_startCpuTimeOverrides[i].value > 100 && g_startCpuTimeOverrides[i].value < 200) {
                    cpuTime = CPUTIME_MULTI_MASK | 80; // "multi", max 80%
                    currentValueToSet = g_startCpuTimeOverrides[i].value - 100;
                } else {
                    cpuTime = CPUTIME_SINGLE_MASK | 80; // "single", max 80%
                    currentValueToSet = g_startCpuTimeOverrides[i].value;
                }
            }
        }
    }

    // Set core1 scheduling mode
    assertSuccess(svcKernelSetState(6, 3, (cpuTime & CPUTIME_MULTI_MASK) ? 0LL : 1LL));

    // Set max value (limit)
    g_manager.maxAppCpuTime = cpuTime & 0x7F;

    // Set current value (for 1.0 apps)
    if (currentValueToSet != 0) {
        assertSuccess(setAppCpuTimeLimit(currentValueToSet));
    }
}

Result SetAppResourceLimit(u32 mbz, ResourceLimitType category, u32 value, u64 mbz2)
{
    if (mbz != 0 || mbz2 != 0 || category != RESLIMIT_CPUTIME || value > (u32)g_manager.maxAppCpuTime) {
        return 0xD8E05BF4;
    }

    value += value < 5 ? 0 : g_manager.cpuTimeBase;
    return setAppCpuTimeLimit(value);
}

Result GetAppResourceLimit(s64 *value, u32 mbz, ResourceLimitType category, u32 mbz2, u64 mbz3)
{
    if (mbz != 0 || mbz2 != 0 || mbz3 != 0 || category != RESLIMIT_CPUTIME) {
        return 0xD8E05BF4;
    }

    Result res = getAppCpuTimeLimit(value);
    if (R_SUCCEEDED(res) && *value >= 5) {
        *value = *value >= g_manager.cpuTimeBase ? *value - g_manager.cpuTimeBase : 0;
    }

    return res;
}
