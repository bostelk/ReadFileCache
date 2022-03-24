// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <distormx.h>
#include <string>
#include <chrono>
#include <filesystem>
#include "src/filereadcache.h"
#include "src/timer.h"
#include "src/logfile.h"

#define is_aligned(POINTER, BYTE_COUNT) \
    (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

#if defined(_UNICODE)
#define _T(x) L ##x
#else
#define _T(x) x
#endif

BOOL WINAPI ReadFileTrace(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
BOOL WINAPI ReadFileCache(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
BOOL(WINAPI* ReadFileOriginal)(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) = NULL;

file_read_cache_t FileReadCache = {};

std::string LogFilePath = "";

// Shortcut.
void AppendLog(std::string message)
{
    AppendLog(LogFilePath, message);
}

std::string GetFilePath(HANDLE hFile)
{
    TCHAR PathW[2048];
    memset(PathW, 0, 2048);
    GetFinalPathNameByHandle(hFile, PathW, 2048, VOLUME_NAME_DOS);

    CHAR Path[2048];
    memset(Path, 0, 2048);
    WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, PathW, 2048, Path, 2048, NULL, NULL);

    return std::string(Path);
}

size_t GetFilePosition(HANDLE hFile)
{
    size_t highPos = 0;
    int lowPos = SetFilePointer(hFile, 0, (PLONG)&highPos, FILE_CURRENT);
    highPos = highPos << 32 | lowPos;
    return highPos;
}

std::string GetModuleDirectory(HMODULE hModule)
{
    char buffer[2048];
    GetModuleFileNameA(hModule, buffer, 2048);
    AppendLog(std::string(buffer));

    std::string filename(buffer);
    std::string directory;
    const size_t last_slash_idx = filename.rfind('\\');
    if (std::string::npos != last_slash_idx)
    {
        directory = filename.substr(0, last_slash_idx);
    }

    return directory;
}

BOOL IsReadFileHooked()
{
    return ReadFileOriginal != NULL;
}

int HookReadFile()
{
    if (IsReadFileHooked())
    {
        AppendLog("Skip because hooked");
        return 1;
    }

    AppendLog("Load Kernel32");
    HMODULE hKernel32 = LoadLibrary(_T("Kernel32.dll"));
    if (hKernel32 == NULL)
    {
        AppendLog("Failed to load module");
        return 1;
    }

    AppendLog("Find ReadFile");
    ULONG_PTR pReadFile = (ULONG_PTR)GetProcAddress(hKernel32, "ReadFile");
    if (pReadFile == NULL)
    {
        AppendLog("Failed to find function");
        return 1;
    }

    AppendLog("Hook");
    if (!distormx_hook((void**)&pReadFile, (void*)&ReadFileCache))
    {
        AppendLog("Failed Hook");
        return 1;
    }

    ReadFileOriginal = (BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED)) pReadFile;
    return 0;
}

int UnhookReadFile()
{
    if (!IsReadFileHooked())
    {
        AppendLog("Skip because not hooked");
        return 1;
    }

    AppendLog("Unhook");
    distormx_unhook((void*)&ReadFileOriginal);
    distormx_destroy();

    ReadFileOriginal = NULL;

    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        // Directory path to "MyDll.dll".
        std::string directory = GetModuleDirectory(hModule);

        // Write log file next to DLL.
        LogFilePath = directory + "\\log.txt";

        AppendLog("Process Attach");

        //HookReadFile();
        break;
    }
    case DLL_THREAD_ATTACH:
    {
        AppendLog("Thread Attach");
        HookReadFile();
        break;
    }
    case DLL_THREAD_DETACH:
    {
        AppendLog("Thread Detach");
        //UnhookReadFile();
        break;
    }
    case DLL_PROCESS_DETACH:
    {
        AppendLog("Process Detach");
        //UnhookReadFile();
        break;
    }
    }
    return TRUE;
}

BOOL WINAPI ReadFileTrace(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
    std::string path = GetFilePath(hFile);

    size_t position = GetFilePosition(hFile);

    timer_t readTimer = {};
    TimerStart(readTimer);

    BOOL ret = ReadFileOriginal(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);

    TimerStop(readTimer);

    /*
    if (lpOverlapped != NULL)
    {
        AppendLog("Overlapped");
    }
    */

    std::string message = "ReadFile," + path + "," + std::to_string(position) + "," + std::to_string(nNumberOfBytesToRead) + "," + std::to_string(TimerElapsed(readTimer).QuadPart);
    AppendLog(message);

    return ret;
}

BOOL WINAPI ReadFileCache(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
    std::string path = GetFilePath(hFile);

    size_t position = GetFilePosition(hFile);

    if (CachePath(path))
    {
        cache_key_t key { path, position, nNumberOfBytesToRead };

        if (!CacheExist(FileReadCache, key))
        {
            AppendLog("CacheMiss," + path + "," + std::to_string(position) + "," + std::to_string(nNumberOfBytesToRead));

            BOOL ret = ReadFileOriginal(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
            if (ret == TRUE)
            {
                //AppendLog("BytesRead," + std::to_string(nNumberOfBytesToRead) + "," + std::to_string(*lpNumberOfBytesRead) + "," + std::to_string(nNumberOfBytesToRead == *lpNumberOfBytesRead) );

                buffer_t buffer = {};
                buffer.BaseAddress = malloc(nNumberOfBytesToRead);
                buffer.SizeBytes = nNumberOfBytesToRead;

                // Zero init.
                //memset(buffer.BaseAddress, 0, buffer.SizeBytes);

                //AppendLog("ReadFileAlignement," + std::to_string(is_aligned(buffer.BaseAddress, 4)) + "," + std::to_string(is_aligned(buffer.BaseAddress, 8)) + "," + std::to_string(is_aligned(buffer.BaseAddress, 16)) + "," + std::to_string(is_aligned(buffer.BaseAddress, 32)));
                //AppendLog("MallocAlignement," + std::to_string(is_aligned(lpBuffer, 4)) + "," + std::to_string(is_aligned(lpBuffer, 8)) + "," + std::to_string(is_aligned(lpBuffer, 16)) + "," + std::to_string(is_aligned(lpBuffer, 32)));

                if (buffer.BaseAddress != NULL)
                {
                    timer_t insertTimer = {};
                    TimerStart(insertTimer);

                    memcpy(buffer.BaseAddress, lpBuffer, buffer.SizeBytes);

                    cache_value_t value = {};
                    value.Buffer = buffer;
                    CacheInsert(FileReadCache, key, value);

                    TimerStop(insertTimer);

                    AppendLog("CacheInsert," + path + "," + std::to_string(position) + "," + std::to_string(nNumberOfBytesToRead) + "," + std::to_string(FileReadCache.SizeBytes) + "," + std::to_string(FileReadCache.Table.size()) + "," + std::to_string(TimerElapsed(insertTimer).QuadPart));
                    //AppendLog("Return," + std::string(Path));
                    return TRUE;
                }
                else
                {
                    //AppendLog("Failed CacheWrite," + std::string(Path));
                    return FALSE;
                }
            }
            else
            {
                //AppendLog("Failed ReadFile," + std::string(Path));
                return FALSE;
            }
        }
        else
        {
            AppendLog("CacheHit," + path + "," + std::to_string(position) + "," + std::to_string(nNumberOfBytesToRead));

            timer_t getTimer = {};
            TimerStart(getTimer);

            cache_value_t value = CacheGet(FileReadCache, key);
            if (value.Buffer.BaseAddress != nullptr)
            {
                if (value.Buffer.BaseAddress != NULL)
                {
                    *lpNumberOfBytesRead = value.Buffer.SizeBytes;

                    memcpy(lpBuffer, value.Buffer.BaseAddress, value.Buffer.SizeBytes);

                    SetFilePointer(hFile, value.Buffer.SizeBytes, NULL, FILE_CURRENT);

                    TimerStop(getTimer);

                    AppendLog("CacheRead," + path + "," + std::to_string(position) + "," + std::to_string(nNumberOfBytesToRead) + "," + std::to_string(FileReadCache.SizeBytes) + "," + std::to_string(FileReadCache.Table.size()) + "," + std::to_string(TimerElapsed(getTimer).QuadPart));
                    //AppendLog("Return," + std::string(Path));
                    return TRUE;
                }
                else
                {
                    //AppendLog("Failed CacheRead," + std::string(Path));
                    return FALSE;
                }
            }
            else
            {
                //AppendLog("Failed CacheFind," + std::string(Path));
            }
        }
    }
    else
    {
        timer_t readTimer = {};
        TimerStart(readTimer);

        BOOL ret = ReadFileOriginal(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
        
        TimerStop(readTimer);

        std::string message = "ReadFile," + path + "," + std::to_string(position) + "," + std::to_string(nNumberOfBytesToRead) + "," + std::to_string(TimerElapsed(readTimer).QuadPart);
        AppendLog(message);

        return ret;
    }
}
