/*   This paricular file is licensed under the following terms: */

/*
*   This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable
*   for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it
*   and redistribute it freely, subject to the following restrictions:
*
*    The origin of this software must not be misrepresented; you must not claim that you wrote the original software.
*    If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
*
*    Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
*    This notice may not be removed or altered from any source distribution.
*/

#pragma once

#include <3ds/types.h>

/// Operations for svcControlService
typedef enum ServiceOp
{
    SERVICEOP_STEAL_CLIENT_SESSION = 0, ///< Steal a client session given a service or global port name
    SERVICEOP_GET_NAME,                 ///< Get the name of a service or global port given a client or session handle
} ServiceOp;

/**
 * @brief Executes a function in supervisor mode, using the supervisor-mode stack.
 * @param func Function to execute.
 * @param ... Function parameters, up to 3 registers.
*/
Result svcCustomBackdoor(void *func, ...);

///@name I/O
///@{
/**
 * @brief Gives the physical address corresponding to a virtual address.
 * @param VA Virtual address.
 * @param writeCheck whether to check if the VA is writable in supervisor mode
 * @return The corresponding physical address, or NULL.
*/
u32 svcConvertVAToPA(const void *VA, bool writeCheck);

/**
 * @brief Flushes a range of the data cache (L2C included).
 * @param addr Start address.
 * @param len Length of the range.
*/
void svcFlushDataCacheRange(void *addr, u32 len);

/**
 * @brief Flushes the data cache entirely (L2C included).
*/
void svcFlushEntireDataCache(void);

/**
 * @brief Invalidates a range of the instruction cache.
 * @param addr Start address.
 * @param len Length of the range.
*/
void svcInvalidateInstructionCacheRange(void *addr, u32 len);

/**
 * @brief Invalidates the data cache entirely.
*/
void svcInvalidateEntireInstructionCache(void);
///@}

///@name Memory management
///@{
/**
 * @brief Maps a block of process memory.
 * @param process Handle of the process.
 * @param destAddress Address of the mapped block in the current process.
 * @param srcAddress Address of the mapped block in the source process.
 * @param size Size of the block of the memory to map (truncated to a multiple of 0x1000 bytes).
*/
Result svcMapProcessMemoryEx(Handle process, u32 destAddr, u32 srcAddr, u32 size);

/**
 * @brief Unmaps a block of process memory.
 * @param process Handle of the process.
 * @param destAddress Address of the block of memory to unmap, in the current (destination) process.
 * @param size Size of the block of memory to unmap (truncated to a multiple of 0x1000 bytes).
 */
Result svcUnmapProcessMemoryEx(Handle process, u32 destAddress, u32 size);

/**
 * @brief Controls memory mapping, with the choice to use region attributes or not.
 * @param[out] addr_out The virtual address resulting from the operation. Usually the same as addr0.
 * @param addr0    The virtual address to be used for the operation.
 * @param addr1    The virtual address to be (un)mirrored by @p addr0 when using @ref MEMOP_MAP or @ref MEMOP_UNMAP.
 *                 It has to be pointing to a RW memory.
 *                 Use NULL if the operation is @ref MEMOP_FREE or @ref MEMOP_ALLOC.
 * @param size     The requested size for @ref MEMOP_ALLOC and @ref MEMOP_ALLOC_LINEAR.
 * @param op       Operation flags. See @ref MemOp.
 * @param perm     A combination of @ref MEMPERM_READ and @ref MEMPERM_WRITE. Using MEMPERM_EXECUTE will return an error.
 *                 Value 0 is used when unmapping memory.
 * @param isLoader Whether to use the region attributes
 * If a memory is mapped for two or more addresses, you have to use MEMOP_UNMAP before being able to MEMOP_FREE it.
 * MEMOP_MAP will fail if @p addr1 was already mapped to another address.
 *
 * @sa svcControlMemory
 */
Result svcControlMemoryEx(u32* addr_out, u32 addr0, u32 addr1, u32 size, MemOp op, MemPerm perm, bool isLoader);
///@}

///@name System
///@{
/**
 * @brief Performs actions related to services or global handles.
 * @param op The operation to perform, see @ref ServiceOp.
 *
 * Examples:
 *     svcControlService(SERVICEOP_GET_NAME, (char [12])outName, (Handle)clientOrSessionHandle);
 *     svcControlService(SERVICEOP_STEAL_CLIENT_SESSION, (Handle *)&outHandle, (const char *)name);
 */
Result svcControlService(ServiceOp op, ...);

/**
 * @brief Copy a handle from a process to another one.
 * @param[out] out The output handle.
 * @param outProcess Handle of the process of the output handle.
 * @param in The input handle. Pseudo-handles are not accepted.
 * @param inProcess Handle of the process of the input handle.
*/
Result svcCopyHandle(Handle *out, Handle outProcess, Handle in, Handle inProcess);

/**
 * @brief Get the address and class name of the underlying kernel object corresponding to a handle.
 * @param[out] outKAddr The output kernel address.
 * @param[out] outName Output class name. The buffer should be large enough to contain it.
 * @param in The input handle.
*/
Result svcTranslateHandle(u32 *outKAddr, char *outClassName, Handle in);
///@}
