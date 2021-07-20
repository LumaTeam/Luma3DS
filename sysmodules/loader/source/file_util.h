#pragma once

#include <optional>
#include <string.h>

extern "C"
{
#include <3ds/result.h>
#include <3ds/services/fs.h>
#include <3ds/svc.h>
#include <3ds/types.h>
}

namespace util
{
inline FS_Path MakePath(const char *path)
{
    return fsMakePath(PATH_ASCII, path);
}

// A small wrapper to make forgetting to close a file and
// to check read lengths impossible.
class File
{
public:
    File() = default;
    File(const File &other) = delete;
    File &operator=(const File &) = delete;
    File(File &&other) { *this = std::move(other); }
    File &operator=(File &&other)
    {
        std::swap(m_handle, other.m_handle);
        return *this;
    }

    ~File() { Close(); }

    bool Close()
    {
        const bool ok = !m_handle || R_SUCCEEDED(FSFILE_Close(*m_handle));
        if(ok)
            m_handle = std::nullopt;
        return ok;
    }

    bool Open(const char *path, int open_flags)
    {
        const FS_Path archive_path = {PATH_EMPTY, 1, ""};
        Handle handle;
        const bool ok = R_SUCCEEDED(FSUSER_OpenFileDirectly(&handle, ARCHIVE_SDMC, archive_path,
                                                            MakePath(path), open_flags, 0));
        if(ok)
            m_handle = handle;
        return ok;
    }

    bool Read(void *buffer, u32 size, u64 offset)
    {
        u32 bytes_read = 0;
        const Result res = FSFILE_Read(*m_handle, &bytes_read, offset, buffer, size);
        return R_SUCCEEDED(res) && bytes_read == size;
    }

    std::optional<u64> GetSize() const
    {
        u64 size;
        if(!R_SUCCEEDED(FSFILE_GetSize(*m_handle, &size)))
            return std::nullopt;
        return size;
    }

private:
    std::optional<Handle> m_handle;
};

}  // namespace util
