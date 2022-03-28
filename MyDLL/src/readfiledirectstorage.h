#pragma once
#include "Windows.h"

// Hook function.
BOOL WINAPI ReadFileDirectStorage(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);