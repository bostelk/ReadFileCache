// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <string>
#include "sys.h"
#include "logfile.h"
#include "readfileoriginal.h"
#include "readfiletrace.h"
#include "readfilecache.h"
#include "readfiledirectstorage.h"
#include "readfilemapped.h"
#include <detours.h>

);
);
);
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  dwReason,
                       LPVOID lpReserved
                     )
{
    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    if (dwReason == DLL_PROCESS_ATTACH) {
        LogInit(hModule);
        AppendLog("Process Attach");

        DetourRestoreAfterWith();

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)ReadFileOriginal, (void*)&ReadFileTrace);
        DetourAttach(&(PVOID&)CreateFileMappingAOriginal, (void*)&CreateFileMappingADetour);
        DetourAttach(&(PVOID&)CreateFileMappingWOriginal, (void*)&CreateFileMappingWDetour);
        DetourAttach(&(PVOID&)MapViewOfFileOriginal, (void*)&MapViewOfFileDetour);
        DetourAttach(&(PVOID&)UnmapViewOfFileOriginal, (void*)&UnmapViewOfFileDetour);
        LONG error = DetourTransactionCommit();
        if (error == NO_ERROR) 
        {
            AppendLog("MyDll" + std::string(DETOURS_STRINGIFY(DETOURS_BITS)) + ".dll: Detoured ReadFile().");
        }
        else 
        {
            AppendLog("MyDll" + std::string(DETOURS_STRINGIFY(DETOURS_BITS)) + ".dll: Error detouring ReadFile(): " + std::to_string(error));
        }
    }
    else if (dwReason == DLL_PROCESS_DETACH) {
        AppendLog("Process Detach");

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)ReadFileOriginal, (void*)&ReadFileTrace);
        DetourDetach(&(PVOID&)CreateFileMappingAOriginal, (void*)&CreateFileMappingADetour);
        DetourDetach(&(PVOID&)CreateFileMappingWOriginal, (void*)&CreateFileMappingWDetour);
        DetourDetach(&(PVOID&)MapViewOfFileOriginal, (void*)&MapViewOfFileDetour);
        DetourDetach(&(PVOID&)UnmapViewOfFileOriginal, (void*)&UnmapViewOfFileDetour);
        DetourTransactionCommit();
    }


    }
    {
    }

{

{
}

{
    return ret;
}

{


    return ret;
}

{
