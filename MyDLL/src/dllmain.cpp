// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <distormx.h>
#include <string>
#include "sys.h"
#include "logfile.h"
#include "readfileoriginal.h"
#include "readfiletrace.h"
#include "readfilecache.h"

#define is_aligned(POINTER, BYTE_COUNT) \
    (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

#if defined(_UNICODE)
#define _T(x) L ##x
#else
#define _T(x) x
#endif

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

        struct tm localtime;
        __time64_t nowtime;

        // Get time as 64-bit integer.
        _time64(&nowtime);
        // Convert to local time.
        _localtime64_s(&localtime, &nowtime);

        char mbstr[100];
        std::strftime(mbstr, sizeof(mbstr), "%F %H-%M-%S", &localtime);

        // Write log file next to DLL.
        LogFilePath = directory + "\\log-" + std::string(mbstr) + ".txt";

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
