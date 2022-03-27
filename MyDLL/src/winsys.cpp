#include "pch.h"
#include "sys.h"
#include "logfile.h"
#include "Windows.h"

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