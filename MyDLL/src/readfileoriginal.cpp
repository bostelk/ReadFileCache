#include "pch.h"
#include "readfileoriginal.h"

BOOL(WINAPI* ReadFileOriginal)(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) = NULL;