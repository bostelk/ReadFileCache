#include "pch.h"
#include "filereadcache.h"
#include "Windows.h"
#include <vector>
#include <algorithm>
#include <assert.h>

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