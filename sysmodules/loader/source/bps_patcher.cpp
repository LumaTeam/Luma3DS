#include "bps_patcher.h"

#include <array>
#include <cstring>
#include <optional>
#include <string_view>
#include <type_traits>

extern "C"
{
#include <3ds/os.h>
#include <3ds/result.h>
#include <3ds/services/fs.h>
#include <3ds/svc.h>

#include "patcher.h"
#include "strings.h"
}

#include "file_util.h"

namespace patcher
{
namespace Bps
{
// The BPS format uses variable length encoding for all integers.
// Realistically uint32s are more than enough for code patching.
using Number = u32;

constexpr std::size_t FooterSize = 12;

// The BPS format uses CRC32 checksums.
[[gnu::optimize("Os")]] static u32 crc32(const u8 *data, std::size_t size)
{
    u32 crc = 0xFFFFFFFF;
    for(std::size_t i = 0; i < size; ++i)
    {
        crc ^= data[i];
        for(std::size_t j = 0; j < 8; ++j)
        {
            u32 mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~crc;
}

// Utility class to make keeping track of offsets and bound checks less error prone.
template <typename T>
class Stream
{
public:
    Stream(T *ptr, std::size_t size) : m_ptr{ptr}, m_size{size} {}

    bool Read(void *buffer, std::size_t length)
    {
        if(m_offset + length > m_size)
            return false;
        std::memcpy(buffer, m_ptr + m_offset, length);
        m_offset += length;
        return true;
    }

    template <typename OtherType>
    [[gnu::optimize("Os")]] bool CopyFrom(Stream<OtherType> &other, std::size_t length)
    {
        if(m_offset + length > m_size)
            return false;
        if(!other.Read(m_ptr + m_offset, length))
            return false;
        m_offset += length;
        return true;
    }

    template <typename ValueType>
    std::optional<ValueType> Read()
    {
        static_assert(std::is_pod_v<ValueType>);
        ValueType val{};
        if(!Read(&val, sizeof(val)))
            return std::nullopt;
        return val;
    }

    [[gnu::optimize("Os")]] Number ReadNumber()
    {
        Number data = 0, shift = 1;
        std::optional<u8> x;
        while((x = Read<u8>()))
        {
            data += (*x & 0x7f) * shift;
            if(*x & 0x80)
                break;
            shift <<= 7;
            data += shift;
        }
        return data;
    }

    auto data() const { return m_ptr; }
    std::size_t size() const { return m_size; }
    std::size_t Tell() const { return m_offset; }

    bool Seek(size_t offset)
    {
        m_offset = offset;
        return true;
    }

private:
    T *m_ptr = nullptr;
    std::size_t m_size = 0;
    std::size_t m_offset = 0;
};

class PatchApplier
{
public:
    PatchApplier(Stream<const u8> source, Stream<u8> target, Stream<const u8> patch)
        : m_source{source}, m_target{target}, m_patch{patch}
    {
    }

    [[gnu::always_inline]] bool Apply()
    {
        const auto magic = *m_patch.Read<std::array<char, 4>>();
        if(std::string_view(magic.data(), magic.size()) != "BPS1")
            return false;

        const Bps::Number source_size = m_patch.ReadNumber();
        const Bps::Number target_size = m_patch.ReadNumber();
        const Bps::Number metadata_size = m_patch.ReadNumber();
        if(source_size > m_source.size() || target_size > m_target.size() || metadata_size != 0)
            return false;

        const std::size_t command_start_offset = m_patch.Tell();
        const std::size_t command_end_offset = m_patch.size() - FooterSize;
        m_patch.Seek(command_end_offset);
        const u32 source_crc32 = *m_patch.Read<u32>();
        const u32 target_crc32 = *m_patch.Read<u32>();
        m_patch.Seek(command_start_offset);

        if(crc32(m_source.data(), source_size) != source_crc32)
            return false;

        // Process all patch commands.
        std::memset(m_target.data(), 0, m_target.size());
        while(m_patch.Tell() < command_end_offset)
        {
            const bool ok = HandleCommand();
            if(!ok)
                return false;
        }

        return crc32(m_target.data(), target_size) == target_crc32;
    }

private:
    bool HandleCommand()
    {
        const Number data = m_patch.ReadNumber();
        const Number command = data & 3;
        const Number length = (data >> 2) + 1;

        switch(command)
        {
        case 0:
            return SourceRead(length);
        case 1:
            return TargetRead(length);
        case 2:
            return SourceCopy(length);
        case 3:
            return TargetCopy(length);
        default:
            return false;
        }
    }

    bool SourceRead(Number length)
    {
        return m_source.Seek(m_target.Tell()) && m_target.CopyFrom(m_source, length);
    }

    bool TargetRead(Number length) { return m_target.CopyFrom(m_patch, length); }

    bool SourceCopy(Number length)
    {
        const Number data = m_patch.ReadNumber();
        m_source_relative_offset += (data & 1 ? -1 : +1) * int(data >> 1);
        if(!m_source.Seek(m_source_relative_offset) || !m_target.CopyFrom(m_source, length))
            return false;
        m_source_relative_offset += length;
        return true;
    }

    bool TargetCopy(Number length)
    {
        const Number data = m_patch.ReadNumber();
        m_target_relative_offset += (data & 1 ? -1 : +1) * int(data >> 1);
        if(m_target.Tell() + length > m_target.size())
            return false;
        if(m_target_relative_offset + length > m_target.size())
            return false;
        // Byte by byte copy.
        for(size_t i = 0; i < length; ++i)
            m_target.data()[m_target.Tell() + i] = m_target.data()[m_target_relative_offset++];
        m_target.Seek(m_target.Tell() + length);
        return true;
    }

    std::size_t m_source_relative_offset = 0;
    std::size_t m_target_relative_offset = 0;
    Stream<const u8> m_source;
    Stream<u8> m_target;
    Stream<const u8> m_patch;
};

}  // namespace Bps

class ScopedAppHeap
{
public:
    ScopedAppHeap()
    {
        u32 tmp;
        m_size = osGetMemRegionFree(MEMREGION_APPLICATION);
        if(!R_SUCCEEDED(svcControlMemory(&tmp, BaseAddress, 0, m_size,
                                         MemOp(MEMOP_ALLOC | MEMOP_REGION_APP),
                                         MemPerm(MEMPERM_READ | MEMPERM_WRITE))))
        {
            svcBreak(USERBREAK_PANIC);
        }
    }

    ~ScopedAppHeap()
    {
        u32 tmp;
        svcControlMemory(&tmp, BaseAddress, 0, m_size, MEMOP_FREE, MemPerm(0));
    }

    static constexpr u32 BaseAddress = 0x08000000;

private:
    u32 m_size;
};

static inline bool ApplyCodeBpsPatch(u64 prog_id, u8 *code, u32 size)
{
    char bps_path[] = "/luma/titles/0000000000000000/code.bps";
    progIdToStr(bps_path + 28, prog_id);
    util::File patch_file;
    if(!patch_file.Open(bps_path, FS_OPEN_READ))
        return true;
    const u32 patch_size = u32(patch_file.GetSize().value_or(0));

    // Temporarily use APPLICATION memory to store the source and patch data.
    ScopedAppHeap memory;

    u8 *source_data = reinterpret_cast<u8 *>(memory.BaseAddress);
    u8 *patch_data = source_data + size;
    std::memcpy(source_data, code, size);
    if(!patch_file.Read(patch_data, patch_size, 0))
        return false;

    Bps::Stream<const u8> source_stream{source_data, size};
    Bps::Stream target_stream{code, size};
    Bps::Stream<const u8> patch_stream{patch_data, patch_size};
    Bps::PatchApplier applier{source_stream, target_stream, patch_stream};
    if(!applier.Apply())
        svcBreak(USERBREAK_PANIC);
    return true;
}

}  // namespace patcher

extern "C"
{
    bool patcherApplyCodeBpsPatch(u64 progId, u8 *code, u32 size)
    {
        return patcher::ApplyCodeBpsPatch(progId, code, size);
    }
}
