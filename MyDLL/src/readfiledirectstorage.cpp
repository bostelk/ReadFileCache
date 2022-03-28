#include "pch.h"
#include "logfile.h"
#include "readfiledirectstorage.h"
#include "sys.h"
#include <dstorage.h>
#include <dxgi1_4.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <winrt/base.h>

using winrt::com_ptr;
using winrt::check_hresult;

struct handle_closer
{
    void operator()(HANDLE h) noexcept
    {
        assert(h != INVALID_HANDLE_VALUE);
        if (h)
        {
            CloseHandle(h);
        }
    }
};
using ScopedHandle = std::unique_ptr<void, handle_closer>;

void GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter)
{
    *ppAdapter = nullptr;
    for (UINT adapterIndex = 0; ; ++adapterIndex)
    {
        IDXGIAdapter1* pAdapter = nullptr;
        if (DXGI_ERROR_NOT_FOUND == pFactory->EnumAdapters1(adapterIndex, &pAdapter))
        {
            // No more adapters to enumerate.
            break;
        }

        // Check to see if the adapter supports Direct3D 12, but don't create the
        // actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr)))
        {
            *ppAdapter = pAdapter;
            return;
        }
        pAdapter->Release();
    }
}

// GlobalDevice
com_ptr<ID3D12Device> gDevice;

com_ptr<ID3D12Device> CreateGlobalDevice()
{
    if (gDevice == nullptr)
    {
        com_ptr<IDXGIFactory4> factory1;
        check_hresult(CreateDXGIFactory1(IID_PPV_ARGS(&factory1)));

        com_ptr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory1.get(), hardwareAdapter.put());

        com_ptr<ID3D12Device> device;
        check_hresult(D3D12CreateDevice(hardwareAdapter.get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device)));

        gDevice = device;
    }

    return gDevice;
}

BOOL WINAPI ReadFileDirectStorage(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
    std::wstring path = GetFilePathW(hFile);

    size_t position = GetFilePosition(hFile);

    com_ptr<ID3D12Device> device = CreateGlobalDevice();

    com_ptr<IDStorageFactory> factory;
    check_hresult(DStorageGetFactory(IID_PPV_ARGS(factory.put())));

    com_ptr<IDStorageFile> file;
    HRESULT hr = factory->OpenFile(path.c_str(), IID_PPV_ARGS(file.put()));
    if (FAILED(hr))
    {
        AppendLog("Failed to open file");
        //AppendLog("The file '" << path << L"' could not be opened. HRESULT=0x" << std::hex << hr);
        return FALSE;
    }

    BY_HANDLE_FILE_INFORMATION info{};
    check_hresult(file->GetFileInformation(&info));
    uint32_t fileSize = info.nFileSizeLow;

    // Create a DirectStorage queue which will be used to load data into a
    // buffer on the GPU.
    DSTORAGE_QUEUE_DESC queueDesc{};
    queueDesc.Capacity = DSTORAGE_MAX_QUEUE_CAPACITY;
    queueDesc.Priority = DSTORAGE_PRIORITY_NORMAL;
    queueDesc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    queueDesc.Device = device.get();

    com_ptr<IDStorageQueue> queue;
    check_hresult(factory->CreateQueue(&queueDesc, IID_PPV_ARGS(queue.put())));

    // Enqueue a request to read the file contents into a destination D3D12 buffer resource.
    // Note: The example request below is performing a single read of the entire file contents.
    DSTORAGE_REQUEST request = {};
    request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MEMORY;
    request.Source.File.Source = file.get();
    request.Source.File.Offset = position;
    request.Source.File.Size = nNumberOfBytesToRead;
    request.UncompressedSize = nNumberOfBytesToRead;
    request.Destination.Memory.Buffer = (void*)lpBuffer;
    request.Destination.Memory.Size = nNumberOfBytesToRead;

    queue->EnqueueRequest(&request);

    // Configure a fence to be signaled when the request is completed
    com_ptr<ID3D12Fence> fence;
    check_hresult(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.put())));

    ScopedHandle fenceEvent(CreateEvent(nullptr, FALSE, FALSE, nullptr));
    constexpr uint64_t fenceValue = 1;
    check_hresult(fence->SetEventOnCompletion(fenceValue, fenceEvent.get()));
    queue->EnqueueSignal(fence.get(), fenceValue);

    // Tell DirectStorage to start executing all queued items.
    queue->Submit();

    // Wait for the submitted work to complete
    AppendLog("Waiting for the DirectStorage request to complete...");
    WaitForSingleObject(fenceEvent.get(), INFINITE);

    // Original ReadFile behaviour.
    {
        *lpNumberOfBytesRead = nNumberOfBytesToRead;
        SetFilePointer(hFile, nNumberOfBytesToRead, NULL, FILE_CURRENT);
    }

    // Check the status array for errors.
    // If an error was detected the first failure record
    // can be retrieved to get more details.
    DSTORAGE_ERROR_RECORD errorRecord{};
    queue->RetrieveErrorRecord(&errorRecord);
    if (FAILED(errorRecord.FirstFailure.HResult))
    {
        //
        // errorRecord.FailureCount - The number of failed requests in the queue since the last
        //                            RetrieveErrorRecord call.
        // errorRecord.FirstFailure - Detailed record about the first failed command in the enqueue order.
        //
        AppendLog("Failed"/*"The DirectStorage request failed! HRESULT=0x" << std::hex << errorRecord.FirstFailure.HResult*/);
        return FALSE;
    }
    else
    {
        AppendLog("The DirectStorage request completed successfully!");
    }

    return TRUE;
}