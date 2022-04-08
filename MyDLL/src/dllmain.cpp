// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <string>
#include "sys.h"
#include "logfile.h"
#include "readfileoriginal.h"
#include "readfiletrace.h"
#include "readfilecache.h"
#include "readfiledirectstorage.h"
#include <detours.h>

HANDLE(WINAPI* CreateFileMappingAOriginal)(
    HANDLE                hFile,
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    DWORD                 flProtect,
    DWORD                 dwMaximumSizeHigh,
    DWORD                 dwMaximumSizeLow,
    LPCSTR                lpName
) = CreateFileMappingA;
HANDLE CreateFileMappingADetour(
    HANDLE                hFile,
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    DWORD                 flProtect,
    DWORD                 dwMaximumSizeHigh,
    DWORD                 dwMaximumSizeLow,
    LPCSTR                lpName
);
HANDLE(WINAPI* CreateFileMappingWOriginal)(
    HANDLE                hFile,
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    DWORD                 flProtect,
    DWORD                 dwMaximumSizeHigh,
    DWORD                 dwMaximumSizeLow,
    LPCWSTR                lpName
    ) = CreateFileMappingW;
HANDLE CreateFileMappingWDetour(
    HANDLE                hFile,
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    DWORD                 flProtect,
    DWORD                 dwMaximumSizeHigh,
    DWORD                 dwMaximumSizeLow,
    LPCWSTR                lpName
);
LPVOID(WINAPI* MapViewOfFileOriginal)(
    HANDLE hFileMappingObject,
    DWORD  dwDesiredAccess,
    DWORD  dwFileOffsetHigh,
    DWORD  dwFileOffsetLow,
    SIZE_T dwNumberOfBytesToMap
) = MapViewOfFile;
LPVOID MapViewOfFileDetour(
    HANDLE hFileMappingObject,
    DWORD  dwDesiredAccess,
    DWORD  dwFileOffsetHigh,
    DWORD  dwFileOffsetLow,
    SIZE_T dwNumberOfBytesToMap
);
BOOL (WINAPI* UnmapViewOfFileOriginal)(
    LPCVOID lpBaseAddress
) = UnmapViewOfFile;
BOOL UnmapViewOfFileDetour(
    LPCVOID lpBaseAddress
);
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  dwReason,
                       LPVOID lpReserved
                     )
{
    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    if (dwReason == DLL_PROCESS_ATTACH) {
        LogInit(hModule);
        AppendLog("Process Attach");

        DetourRestoreAfterWith();

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)ReadFileOriginal, (void*)&ReadFileTrace);
        DetourAttach(&(PVOID&)CreateFileMappingAOriginal, (void*)&CreateFileMappingADetour);
        DetourAttach(&(PVOID&)CreateFileMappingWOriginal, (void*)&CreateFileMappingWDetour);
        DetourAttach(&(PVOID&)MapViewOfFileOriginal, (void*)&MapViewOfFileDetour);
        DetourAttach(&(PVOID&)UnmapViewOfFileOriginal, (void*)&UnmapViewOfFileDetour);
        LONG error = DetourTransactionCommit();
        if (error == NO_ERROR) 
        {
            AppendLog("MyDll" + std::string(DETOURS_STRINGIFY(DETOURS_BITS)) + ".dll: Detoured ReadFile().");
        }
        else 
        {
            AppendLog("MyDll" + std::string(DETOURS_STRINGIFY(DETOURS_BITS)) + ".dll: Error detouring ReadFile(): " + std::to_string(error));
        }
    }
    else if (dwReason == DLL_PROCESS_DETACH) {
        AppendLog("Process Detach");

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)ReadFileOriginal, (void*)&ReadFileTrace);
        DetourDetach(&(PVOID&)CreateFileMappingAOriginal, (void*)&CreateFileMappingADetour);
        DetourDetach(&(PVOID&)CreateFileMappingWOriginal, (void*)&CreateFileMappingWDetour);
        DetourDetach(&(PVOID&)MapViewOfFileOriginal, (void*)&MapViewOfFileDetour);
        DetourDetach(&(PVOID&)UnmapViewOfFileOriginal, (void*)&UnmapViewOfFileDetour);
        DetourTransactionCommit();
    }
    return TRUE;
}
HANDLE CreateFileMappingADetour(
    HANDLE                hFile,
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    DWORD                 flProtect,
    DWORD                 dwMaximumSizeHigh,
    DWORD                 dwMaximumSizeLow,
    LPCSTR                lpName
)
{
    std::string path = GetFilePath(hFile);
    AppendLog("CreateFileMappingA " + path);
    HANDLE ret = CreateFileMappingAOriginal(hFile, lpFileMappingAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
    return ret;
}

struct view_key_t
{
    std::string Path;
    size_t Position;
    size_t Size;
};

bool operator==(const view_key_t& lhs, const view_key_t& rhs);

// custom hash can be a standalone function object:
struct ViewItemHash
{
    std::size_t operator()(view_key_t const& s) const noexcept
    {
        return 0;
        std::size_t h1 = std::hash<std::string>{}(s.Path);
        std::size_t h2 = std::hash<size_t>{}(s.Position);
        std::size_t h3 = std::hash<size_t>{}(s.Size);
        return h1 ^ (h2 << 1) ^ (h3 << 1);
    }
};

template<>
struct std::hash<view_key_t>
{
    std::size_t operator()(view_key_t const& s) const noexcept
    {
        std::size_t h1 = std::hash<std::string>{}(s.Path);
        std::size_t h2 = std::hash<size_t>{}(s.Position);
        std::size_t h3 = std::hash<size_t>{}(s.Size);
        return h1 ^ (h2 << 1) ^ (h3 << 1);
    }
};

struct view_value_t
{
    void* buffer;
};

struct file_map_view_t
{
    std::unordered_map <view_key_t, view_value_t> Table;
    size_t SizeBytes;
    std::mutex Mutex;
};

// Global
file_map_view_t FileMapViewGlobal;
std::unordered_map<HANDLE, HANDLE> FileMapToFile;
std::unordered_map<void*, view_key_t> BufferToFileView;

bool operator==(const view_key_t& lhs, const view_key_t& rhs)
{
    return lhs.Path == rhs.Path && lhs.Position == rhs.Position && lhs.Size == rhs.Size;
}

HANDLE CreateFileMappingWDetour(
    HANDLE                hFile,
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    DWORD                 flProtect,
    DWORD                 dwMaximumSizeHigh,
    DWORD                 dwMaximumSizeLow,
    LPCWSTR                lpName
)
{
    std::string path = GetFilePath(hFile);
    AppendLog("CreateFileMappingW " + path);
    HANDLE ret = CreateFileMappingWOriginal(hFile, lpFileMappingAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
    if (ret != INVALID_HANDLE_VALUE)
    {
        if (path == "\\\\?\\I:\\Path Of Exile\\Content.ggpk")
        {
            std::lock_guard<std::mutex> guard(FileMapViewGlobal.Mutex);
            if (FileMapToFile.count(ret) == 0)
            {
                HANDLE hFileDuplicate = INVALID_HANDLE_VALUE;
                DuplicateHandle(GetCurrentProcess(), hFile, GetCurrentProcess(), &hFileDuplicate, NULL, FALSE, DUPLICATE_SAME_ACCESS);
                FileMapToFile.insert(std::make_pair(ret, hFileDuplicate));
            }
        }
    }
    return ret;
}

LPVOID MapViewOfFileDetour(
    HANDLE hFileMappingObject,
    DWORD  dwDesiredAccess,
    DWORD  dwFileOffsetHigh,
    DWORD  dwFileOffsetLow,
    SIZE_T dwNumberOfBytesToMap
)
{
    size_t offset = (size_t)dwFileOffsetHigh << 32 | dwFileOffsetLow;
    std::string path;

    {
        std::lock_guard<std::mutex> guard(FileMapViewGlobal.Mutex);
        if (FileMapToFile.count(hFileMappingObject) > 0)
        {
            HANDLE hFile = FileMapToFile.at(hFileMappingObject);
            if (hFile == INVALID_HANDLE_VALUE)
            {
                AppendLog("Invalid file handle");
                return nullptr;
            }

            path = GetFilePath(hFile);

            view_key_t key = { path, offset, dwNumberOfBytesToMap };

            if (FileMapViewGlobal.Table.count(key) == 0)
            {
                DWORD lpNumberOfBytesRead = 0;
                void* lpBuffer = malloc(dwNumberOfBytesToMap);
                if (lpBuffer == nullptr)
                {
                    AppendLog("Failed to allocate buffer");
                    return nullptr;
                }

                BufferToFileView.insert(std::make_pair(lpBuffer, key));
                FileMapViewGlobal.SizeBytes += dwNumberOfBytesToMap;

                // Reset to beginning.
                SetFilePointer(hFile, 0, 0, FILE_BEGIN);

                LONG lDistanceToMove = dwFileOffsetLow;
                LONG lDistanceToMoveHigh = dwFileOffsetHigh;

                SetFilePointer(hFile, lDistanceToMove, &lDistanceToMoveHigh, FILE_BEGIN);

                BOOL ret = ReadFile(hFile, lpBuffer, dwNumberOfBytesToMap, &lpNumberOfBytesRead, NULL);
                if (ret)
                {
                    return lpBuffer;
                }
                else
                {
                    AppendLog("Failed to ReadFile");
                }
            }
            else
            {
                return FileMapViewGlobal.Table.at(key).buffer;
            }
        }
    }

    AppendLog("MapViewOfFile," + path + "," + std::to_string(offset) + "," + std::to_string(dwNumberOfBytesToMap));
    LPVOID ret = MapViewOfFileOriginal(hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap);
    return ret;
}

BOOL UnmapViewOfFileDetour(
    LPCVOID lpBaseAddress
)
{
    std::lock_guard<std::mutex> guard(FileMapViewGlobal.Mutex);

    if (BufferToFileView.count((void*)lpBaseAddress) > 0)
    {
        view_key_t view_key = BufferToFileView.at((void*)lpBaseAddress);
        BufferToFileView.erase((void*)lpBaseAddress);
        FileMapViewGlobal.Table.erase(view_key);
        FileMapViewGlobal.SizeBytes -= view_key.Size;
        free((void*)lpBaseAddress);
        return true;
    }
    else
    {
        AppendLog("UnmapViewOfFile");
        BOOL ret = UnmapViewOfFileOriginal(lpBaseAddress);
        return ret;
    }
}