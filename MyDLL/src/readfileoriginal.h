#pragma once
#include "Windows.h"

// Hook function.
extern BOOL(WINAPI* ReadFileOriginal)(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);