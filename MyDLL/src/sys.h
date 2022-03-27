#pragma once
#include <string>
#include "Windows.h"

std::string GetFilePath(HANDLE hFile);
size_t GetFilePosition(HANDLE hFile);
std::string GetModuleDirectory(HMODULE hModule);