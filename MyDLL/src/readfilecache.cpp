#include "pch.h"
#include "readfilecache.h"
#include "readfileoriginal.h"
#include "Windows.h"
#include <vector>
#include <algorithm>
#include <assert.h>
#include "timer.h"
#include "sys.h"
#include "logfile.h"

extern file_read_cache_t FileReadCache = {};

bool operator==(const cache_key_t& lhs, const cache_key_t& rhs)
{
    return lhs.Path == rhs.Path && lhs.Position == rhs.Position && lhs.Size == rhs.Size;
}

bool CacheExist(file_read_cache_t& cache, cache_key_t key)
{
    std::lock_guard<std::mutex> guard(cache.Mutex);
    return cache.Table.count(key) > 0;
}

void CacheInsert(file_read_cache_t& cache, cache_key_t key, cache_value_t value)
{
    std::lock_guard<std::mutex> guard(cache.Mutex);

    if ((cache.SizeBytes + value.Buffer.SizeBytes) >= cache.MaxSizeBytes)
    {
        // Order items by least recently accessed.
        std::vector<std::pair<cache_key_t, cache_value_t>> pairs;
        pairs.reserve(cache.Table.size());

        for (auto pair : cache.Table)
        {
            pairs.push_back(pair);
        }

        std::sort(pairs.begin(), pairs.end(), SortByLastAccesTimeAccesnding());

        // The number of pairs removed.
        int index = 0;

        // Remove items until there is free space in the cache.
        while (cache.Table.size() > 0 &&
              (cache.SizeBytes + value.Buffer.SizeBytes) >= cache.MaxSizeBytes)
        {
            cache.Table.erase(pairs[index].first);
            cache.SizeBytes -= pairs[index].second.Buffer.SizeBytes;

            // Release FileRead buffer.
            if (pairs[index].second.Buffer.BaseAddress)
            {
                free(pairs[index].second.Buffer.BaseAddress);
            }

            index++;
        }
    }

    // There must be space in the cache now.
    assert((cache.SizeBytes + value.Buffer.SizeBytes) < cache.MaxSizeBytes);

    CacheRefreshLastAccess(value);
    cache.Table.insert(std::make_pair(key, value));
    cache.SizeBytes += value.Buffer.SizeBytes;
}

cache_value_t CacheGet(file_read_cache_t& cache, cache_key_t key)
{
    std::lock_guard<std::mutex> guard(cache.Mutex);

    auto pair = cache.Table.find(key);
    if (pair != cache.Table.end())
    {
        CacheRefreshLastAccess(pair->second);
        return pair->second;
    }

    // Empty value.
    return {};
}

void CacheRefreshLastAccess(cache_value_t& value)
{
    QueryPerformanceCounter((LARGE_INTEGER*)&value.LastAccessTime);
}

bool CachePath(std::string path)
{
    // Exclude unknown files.
    bool result = !path.empty();

    // Exclude files that do not affect gameplay.
    result = result && std::string(path).find("Music") == std::string::npos;
    result = result && std::string(path).find("Ambient") == std::string::npos;
    result = result && std::string(path).find("Video") == std::string::npos;

    // Cache FMOD reads because of their frequency.
    // return std::string(path).find("FMOD") != std::string::npos;

    return result;
}

BOOL WINAPI ReadFileCache(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
    std::string path = GetFilePath(hFile);

    size_t position = GetFilePosition(hFile);

    if (CachePath(path))
    {
        cache_key_t key{ path, position, nNumberOfBytesToRead };

        if (!CacheExist(FileReadCache, key))
        {
            timer_t missTimer = {};
            TimerStart(missTimer);

            BOOL ret = ReadFileOriginal(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
            if (ret == TRUE)
            {
                //AppendLog("BytesRead," + std::to_string(nNumberOfBytesToRead) + "," + std::to_string(*lpNumberOfBytesRead) + "," + std::to_string(nNumberOfBytesToRead == *lpNumberOfBytesRead) );

                buffer_t buffer = {};
                buffer.BaseAddress = malloc(nNumberOfBytesToRead);
                buffer.SizeBytes = nNumberOfBytesToRead;

                // Zero init.
                //memset(buffer.BaseAddress, 0, buffer.SizeBytes);

                //AppendLog("ReadFileAlignement," + std::to_string(is_aligned(buffer.BaseAddress, 4)) + "," + std::to_string(is_aligned(buffer.BaseAddress, 8)) + "," + std::to_string(is_aligned(buffer.BaseAddress, 16)) + "," + std::to_string(is_aligned(buffer.BaseAddress, 32)));
                //AppendLog("MallocAlignement," + std::to_string(is_aligned(lpBuffer, 4)) + "," + std::to_string(is_aligned(lpBuffer, 8)) + "," + std::to_string(is_aligned(lpBuffer, 16)) + "," + std::to_string(is_aligned(lpBuffer, 32)));

                if (buffer.BaseAddress != NULL)
                {
                    memcpy(buffer.BaseAddress, lpBuffer, buffer.SizeBytes);

                    cache_value_t value = {};
                    value.Buffer = buffer;

                    timer_t insertTimer = {};
                    TimerStart(insertTimer);

                    CacheInsert(FileReadCache, key, value);

                    TimerStop(insertTimer);
                    TimerStop(missTimer);

                    AppendLog("CacheInsert," + path + "," + std::to_string(position) + "," + std::to_string(nNumberOfBytesToRead) + "," + std::to_string(TimerElapsed(insertTimer).QuadPart) + "," + std::to_string(FileReadCache.SizeBytes) + "," + std::to_string(FileReadCache.Table.size()));
                    AppendLog("CacheMiss," + path + "," + std::to_string(position) + "," + std::to_string(nNumberOfBytesToRead) + "," + std::to_string(TimerElapsed(missTimer).QuadPart) + "," + std::to_string(FileReadCache.SizeBytes) + "," + std::to_string(FileReadCache.Table.size()));
                    //AppendLog("Return," + std::string(Path));
                    return TRUE;
                }
                else
                {
                    //AppendLog("Failed CacheWrite," + std::string(Path));
                    return FALSE;
                }
            }
            else
            {
                //AppendLog("Failed ReadFile," + std::string(Path));
                return FALSE;
            }
        }
        else
        {
            timer_t hitTimer = {};
            TimerStart(hitTimer);

            cache_value_t value = CacheGet(FileReadCache, key);
            if (value.Buffer.BaseAddress != nullptr)
            {
                *lpNumberOfBytesRead = value.Buffer.SizeBytes;

                memcpy(lpBuffer, value.Buffer.BaseAddress, value.Buffer.SizeBytes);

                SetFilePointer(hFile, value.Buffer.SizeBytes, NULL, FILE_CURRENT);

                TimerStop(hitTimer);

                AppendLog("CacheHit," + path + "," + std::to_string(position) + "," + std::to_string(nNumberOfBytesToRead) + "," + std::to_string(TimerElapsed(hitTimer).QuadPart) + "," + std::to_string(FileReadCache.SizeBytes) + "," + std::to_string(FileReadCache.Table.size()));
                //AppendLog("Return," + std::string(Path));
                return TRUE;
            }
            else
            {
                //AppendLog("Failed CacheFind," + std::string(Path));
            }
        }
    }
    else
    {
        timer_t readTimer = {};
        TimerStart(readTimer);

        BOOL ret = ReadFileOriginal(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);

        TimerStop(readTimer);

        std::string message = "ReadFile," + path + "," + std::to_string(position) + "," + std::to_string(nNumberOfBytesToRead) + "," + std::to_string(TimerElapsed(readTimer).QuadPart);
        AppendLog(message);

        return ret;
    }
}