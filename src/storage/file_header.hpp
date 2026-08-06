#pragma once

#include <cstdint>
#include <vector>

namespace vectordb
{
    class FileHeader
    {
    public:
        static FileHeader create();
        std::vector<char> serialise() const;
        static FileHeader deserialise(const std::vector<char>& buffer);
        bool valid() const;

    private:
        char magic[4];
        uint32_t version;
        uint32_t page_size;
        uint64_t page_count;
        uint64_t catalog_root;
        uint8_t clean_shutdown;
    };
}
