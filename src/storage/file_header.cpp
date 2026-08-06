#include "file_header.hpp"
#include "vectordb/config.hpp"
#include <cstring>
#include <stdexcept>

namespace vectordb
{
    FileHeader FileHeader::create()
    {
        FileHeader header{};
        std::memcpy(header.magic, DATABASE_MAGIC, 4);
        header.version = DATABASE_VERSION;
        header.page_size = PAGE_SIZE;
        header.page_count = 1;
        header.catalog_root = 0;
        header.clean_shutdown = 1;

        return header;
    }

    std::vector<char> FileHeader::serialise() const
    {
        std::vector<char> buffer(PAGE_SIZE, 0);
        size_t offset = 0;

        std::memcpy(buffer.data() + offset, magic, sizeof(magic));
        offset += sizeof(magic);

        std::memcpy(buffer.data() + offset, &version, sizeof(version));
        offset += sizeof(version);

        std::memcpy(buffer.data() + offset, &page_size, sizeof(page_size));
        offset += sizeof(page_size);

        std::memcpy(buffer.data() + offset, &page_count, sizeof(page_count));
        offset += sizeof(page_count);

        std::memcpy(buffer.data() + offset, &catalog_root, sizeof(catalog_root));
        offset += sizeof(catalog_root);

        std::memcpy(buffer.data() + offset, &clean_shutdown, sizeof(clean_shutdown));
        offset += sizeof(clean_shutdown);

        return buffer;
    }

    FileHeader FileHeader::deserialise(const std::vector<char>& buffer)
    {
        constexpr size_t min_size =
            sizeof(FileHeader::magic) +
            sizeof(uint32_t) * 2 +
            sizeof(uint64_t) * 2 +
            sizeof(uint8_t);

        if (buffer.size() < min_size)
        {
            throw std::runtime_error("file header buffer too small");
        }

        FileHeader header{};
        size_t offset = 0;

        std::memcpy(header.magic, buffer.data() + offset, sizeof(header.magic));
        offset += sizeof(header.magic);

        std::memcpy(&header.version, buffer.data() + offset, sizeof(header.version));
        offset += sizeof(header.version);

        std::memcpy(&header.page_size, buffer.data() + offset, sizeof(header.page_size));
        offset += sizeof(header.page_size);

        std::memcpy(&header.page_count, buffer.data() + offset, sizeof(header.page_count));
        offset += sizeof(header.page_count);

        std::memcpy(&header.catalog_root, buffer.data() + offset, sizeof(header.catalog_root));
        offset += sizeof(header.catalog_root);

        std::memcpy(&header.clean_shutdown, buffer.data() + offset, sizeof(header.clean_shutdown));
        offset += sizeof(header.clean_shutdown);

        return header;
    }

    bool FileHeader::valid() const
    {
        if (std::memcmp(magic, DATABASE_MAGIC, 4) != 0)
        {
            return false;
        }

        if (version != DATABASE_VERSION)
        {
            return false;
        }

        if (page_size != PAGE_SIZE)
        {
            return false;
        }

        return true;
    }
}
