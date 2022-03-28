#include "pch.h"
#include "sys.h"
#include "logfile.h"
#include "Windows.h"
#include <time.h>
#include <chrono>

std::string LogFilePath = "";

void LogInit(HMODULE hModule)
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
}

// Shortcut.
void AppendLog(std::string message)
{
    AppendLog(LogFilePath, message);
}

void AppendLog(std::string filePath, std::string message)
{
    if (filePath.empty())
    {
        return;
    }

    std::string threadId = std::to_string(GetCurrentThreadId());

    LARGE_INTEGER StartingTime;
    LARGE_INTEGER Frequency;

    QueryPerformanceFrequency(&Frequency);
    QueryPerformanceCounter(&StartingTime);

    // Convert to microseconds.
    StartingTime.QuadPart *= 1000000;
    StartingTime.QuadPart /= Frequency.QuadPart;

    std::string timestamp = std::to_string(StartingTime.QuadPart);

    message = threadId + "," + timestamp + "," + message + "\n";

    void* hLogFile = CreateFileA(filePath.c_str(),  // name of the write
        FILE_APPEND_DATA,          // open for appending
        FILE_SHARE_READ,           // share for reading only
        NULL,                      // default security
        OPEN_ALWAYS,               // open existing file or create new file 
        FILE_ATTRIBUTE_NORMAL,     // normal file
        NULL);
    if (hLogFile != INVALID_HANDLE_VALUE)
    {
        WriteFile(
            hLogFile,              // open file handle
            message.c_str(),               // start of data to write
            message.size(),     // number of bytes to write
            NULL,
            NULL);
        CloseHandle(hLogFile);
    }
}