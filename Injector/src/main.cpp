#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <Windows.h>
#include <string>

#define LOAD_LIBRARY_A "LoadLibraryA"
#define KERNEL32_DLL "kernel32.dll"
#define DLL_FILENAME "MyDLL.dll"

LPVOID GetLoadLibraryAAddress(){
    LPVOID lpAddress = NULL;
    HMODULE hModule = LoadLibraryA(KERNEL32_DLL);
    if (!hModule){
        printf("LoadLibraryA failed to load \"%s\" with error: %lu\n", KERNEL32_DLL, GetLastError());
        return NULL;
    }
    lpAddress = GetProcAddress(hModule, LOAD_LIBRARY_A);
    if (!lpAddress){
        printf("GetProcAddress failed with error: %lu\n", GetLastError());
    }else{
        printf("Got LoadLibraryA's address: %p\n", lpAddress);
    }

    FreeLibrary(hModule);
    return lpAddress;
}

LPVOID WriteDllPathToProcess(HANDLE hProc, const char* dllPath){
    BOOL res = 0;
    LPVOID lpAddress = VirtualAllocEx(hProc, NULL, 2048,
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!lpAddress){
        printf("Failed to allocate memory: VirtualAllocEx failed with error: %lu\n", GetLastError());
        return NULL;
    }
    res = WriteProcessMemory(hProc, lpAddress, dllPath, 2048, NULL);
    if (!res){
        printf("Failed to write to process: WriteProcessMemory failed with error: %lu\n", GetLastError());
        VirtualFreeEx(hProc, lpAddress, 0, MEM_RELEASE);
        return NULL;
    }
    printf("Wrote path \"%s\" successfully to address %p\n", dllPath, lpAddress);

    return lpAddress;
}

BOOL InjectDll(HANDLE hProc){
    LPVOID lpLoadLibraryAddress = NULL;
    LPVOID lpDllPathInProc = NULL;
    HANDLE hThread = INVALID_HANDLE_VALUE;

    // File path to "Injector.exe".
    char buffer[2048];
    GetModuleFileNameA(NULL, buffer, 2048);


    std::string filename(buffer);
    std::string directory;
    const size_t last_slash_idx = filename.rfind('\\');
    if (std::string::npos != last_slash_idx)
    {
        directory = filename.substr(0, last_slash_idx);
    }

    std::string dllFilepath = directory + "\\" + DLL_FILENAME;

    lpLoadLibraryAddress = GetLoadLibraryAAddress();
    if(!lpLoadLibraryAddress){
        printf("GetLoadLibraryAAddress failed!\n");
        return FALSE;
    }
    lpDllPathInProc = WriteDllPathToProcess(hProc, dllFilepath.c_str());
    if(!lpDllPathInProc){
        printf("WriteDllPathToProcess failed!\n");
        return FALSE;
    }
    hThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)lpLoadLibraryAddress, lpDllPathInProc, 0, NULL);
    if(!hThread){
        printf("CreateRemoteThread failed to create thread with error: %lu\n", GetLastError());
        return FALSE;
    }
    printf("Created thread with handle %p\n", hThread);
    printf("Successfuly injected the DLL into the process!\n");
    return TRUE;
}

int main() {
    HANDLE hProc = NULL;
    DWORD pid = 0;
    BOOL res = FALSE;

    printf("Enter pid: ");
    scanf("%lu", &pid);
    hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if(!hProc){
        printf("OpenProcess failed to open process with pid %d with error: %lu\n", pid, GetLastError());
        return -1;
    }
    res = InjectDll(hProc);
    CloseHandle(hProc);
    getchar();
    getchar();
    return 0;
}
