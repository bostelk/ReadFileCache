#include "readfiledirectstorage.h";
#include "sys.h"
#include "logfile.h"
#include <stdio.h>
#include <iostream>

int main()
{
	LogInit(NULL);
	AppendLog("test");

    void* hLogFile = CreateFileA(LogFilePath.c_str(),  // name of the read
        FILE_READ_ACCESS,          // open for reading
        FILE_SHARE_READ,           // share for reading only
        NULL,                      // default security
        OPEN_ALWAYS,               // open existing file or create new file 
        FILE_ATTRIBUTE_NORMAL,     // normal file
        NULL);
    if (hLogFile != INVALID_HANDLE_VALUE)
    {
        uint32_t FileSizeHigh = 0;
        uint32_t FileSizeLow = GetFileSize(hLogFile, (LPDWORD)&FileSizeHigh);

        void* buffer = malloc(FileSizeLow);
        memset(buffer, 0, FileSizeLow);

        uint32_t BytesRead = 0;
        ReadFileDirectStorage(hLogFile, buffer, FileSizeLow, (LPDWORD)&BytesRead, NULL);

        std::string output((char*)buffer, FileSizeLow);
        std::cout << output << std::endl;
    }
}