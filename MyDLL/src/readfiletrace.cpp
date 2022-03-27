#include "pch.h"
#include "readfiletrace.h"
#include "readfileoriginal.h"
#include "sys.h"
#include "logfile.h"
#include "timer.h"

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