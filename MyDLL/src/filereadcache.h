#pragma once
#include <concurrent_unordered_map.h>
#include <string>
#include <mutex>
#include <unordered_map>

struct buffer_t
{
    void* BaseAddress;
    size_t SizeBytes;
};

struct cache_value_t
{
    buffer_t Buffer;
    int64_t LastAccessTime;
};

struct cache_key_t
{
    std::string Path;
    size_t Position;
    size_t Size;
};

bool operator==(const cache_key_t& lhs, const cache_key_t& rhs);

// custom hash can be a standalone function object:
struct CacheItemHash
{
    std::size_t operator()(cache_key_t const& s) const noexcept
    {
        return 0;
        std::size_t h1 = std::hash<std::string>{}(s.Path);
        std::size_t h2 = std::hash<size_t>{}(s.Position);
        std::size_t h3 = std::hash<size_t>{}(s.Size);
        return h1 ^ (h2 << 1) ^ (h3 << 1);
    }
};

template<>
struct std::hash<cache_key_t>
{
    std::size_t operator()(cache_key_t const& s) const noexcept
    {
        std::size_t h1 = std::hash<std::string>{}(s.Path);
        std::size_t h2 = std::hash<size_t>{}(s.Position);
        std::size_t h3 = std::hash<size_t>{}(s.Size);
        return h1 ^ (h2 << 1) ^ (h3 << 1);
    }
};

struct SortByLastAccesTimeAccesnding {
    bool operator()(std::pair<cache_key_t, cache_value_t> const& a, std::pair<cache_key_t, cache_value_t> const& b) const {
        return a.second.LastAccessTime < b.second.LastAccessTime;
    }
};

struct file_read_cache_t
{
    std::unordered_map <cache_key_t, cache_value_t> Table;
    size_t SizeBytes;
    // Default to 500MB.
    size_t MaxSizeBytes = 500 * 1024 * 1024;
    std::mutex Mutex;
};

// Insert new item into cache. Will replace new item according to replacement policy (LRU).
void CacheInsert(file_read_cache_t& cache, cache_key_t key, cache_value_t value);
cache_value_t CacheGet(file_read_cache_t& cache, cache_key_t key);
void CacheRefreshLastAccess(cache_value_t& value);
bool CacheExist(file_read_cache_t& cache, cache_key_t key);
bool CachePath(std::string path);
