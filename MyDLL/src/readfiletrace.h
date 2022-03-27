#pragma once
#include "Windows.h"

// Hook function.
BOOL WINAPI ReadFileTrace(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);