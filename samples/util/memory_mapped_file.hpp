#pragma once

#ifdef _WIN32
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <memoryapi.h>

namespace win32 {

class memory_mapped_file
{
  public:
    memory_mapped_file()                                     noexcept = default;
    memory_mapped_file(memory_mapped_file &other)            noexcept = default;
    memory_mapped_file &operator=(memory_mapped_file &other) noexcept = default;

    memory_mapped_file(memory_mapped_file &&other) noexcept : memory_mapped_file()
    {
        swap(other);
    }

    memory_mapped_file &operator=(memory_mapped_file &&other) noexcept
    {
        swap(other);
        return *this;
    }

    ~memory_mapped_file()
    {
        close();
    }

    uint64_t const size() const
    {
        LARGE_INTEGER size;
        if (!GetFileSizeEx(file_, &size))
            return 0;
        return size.QuadPart;
    }

    void close()
    {
        // make sure to handle partial open failures
        if (base_ptr_) {
            UnmapViewOfFile(base_ptr_);
            base_ptr_ = nullptr;
        }

        if (file_mapping_ != INVALID_HANDLE_VALUE) {
            CloseHandle(file_mapping_);
            file_mapping_ = INVALID_HANDLE_VALUE;
        }

        if (file_ != INVALID_HANDLE_VALUE) {
            CloseHandle(file_);
            file_ = INVALID_HANDLE_VALUE;
        }
    }

    bool open(std::string_view pathname) noexcept
    {
        file_ = CreateFileA(&*pathname.cbegin(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file_ == INVALID_HANDLE_VALUE)
            return false;

        file_mapping_ = CreateFileMapping(file_, NULL, PAGE_READONLY,  0, 0, NULL);
        if (file_mapping_ == NULL)
            return false;

        base_ptr_ = static_cast<decltype(base_ptr_)>(MapViewOfFile(file_mapping_, FILE_MAP_READ, 0, 0, 0));
        if (base_ptr_ == nullptr)
            return false;

        return true;
    }

    operator char const * const() const noexcept
    {
        return base_ptr_;
    }

  private:
    void swap(memory_mapped_file &other) noexcept
    {
        using std::swap;
        swap(file_,         other.file_);
        swap(base_ptr_,     other.base_ptr_);
        swap(file_mapping_, other.file_mapping_);
    }

  private:
    HANDLE      file_         = INVALID_HANDLE_VALUE;
    HANDLE      file_mapping_ = INVALID_HANDLE_VALUE;
    char const *base_ptr_     = nullptr;
};

inline memory_mapped_file map_file(std::string_view pathname)
{
    memory_mapped_file mmf;
    mmf.open(pathname);
    return mmf;
}

}   // namespace win32

#endif  // _WIN32
