#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace vectordb::bytes
{
    template <typename T>
    inline void put(std::vector<char>& buf, T value)
    {
        size_t offset = buf.size();
        buf.resize(offset + sizeof(T));
        std::memcpy(buf.data() + offset, &value, sizeof(T));
    }

    inline void put_bytes(std::vector<char>& buf, const char* data, size_t len)
    {
        buf.insert(buf.end(), data, data + len);
    }

    inline uint32_t fnv1a(const char* data, size_t len)
    {
        uint32_t hash = 2166136261u;
        for (size_t i = 0; i < len; ++i)
        {
            hash ^= static_cast<uint8_t>(data[i]);
            hash *= 16777619u;
        }
        return hash;
    }
}
