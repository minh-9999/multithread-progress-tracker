#pragma once

#include <cstddef>
#include <cstdint>
#include <fileapi.h>
#include <handleapi.h>
#include <memoryapi.h>
#include <span>
#include <string>
#include <winbase.h>
#include <winnt.h>
#include <stdexcept>

using namespace std;

class MemoryMappedFile
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapFile = nullptr;
    uint8_t *data = nullptr;
    size_t fileSize = 0;

public:
    MemoryMappedFile(const string &filePath)
    {
        hFile = CreateFileA(
            filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (hFile == INVALID_HANDLE_VALUE)
            throw runtime_error("Failed to open file");

        LARGE_INTEGER size;
        if (!GetFileSizeEx(hFile, &size))
        {
            CloseHandle(hFile);
            throw runtime_error("Failed to get file size");
        }

        fileSize = static_cast<size_t>(size.QuadPart);

        hMapFile = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!hMapFile)
        {
            CloseHandle(hFile);
            throw runtime_error("Failed to create file mapping");
        }

        data = static_cast<uint8_t *>(MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0));
        if (!data)
        {
            CloseHandle(hMapFile);
            CloseHandle(hFile);
            throw runtime_error("Failed to map view of file");
        }

        // Now it's safe to close hFile
        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }

    span<uint8_t> get_data() const
    {
        return { data, fileSize };
    }

    ~MemoryMappedFile()
    {
        if (data)
            UnmapViewOfFile(data);

        if (hMapFile)
            CloseHandle(hMapFile);

        if (hFile != INVALID_HANDLE_VALUE)
            CloseHandle(hFile); // only if mapping fails
    }
};