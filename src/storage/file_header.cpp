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
        header.m_version = DATABASE_VERSION;
        header.m_page_size = PAGE_SIZE;
        header.m_page_count = 1;
        header.m_catalog_root = 0;
        header.m_clean_shutdown = 1;

        return header;
    }

    std::vector<char> FileHeader::serialise() const
    {
        std::vector<char> buffer(PAGE_SIZE, 0);
        size_t offset = 0;

        std::memcpy(buffer.data() + offset, magic, sizeof(magic));
        offset += sizeof(magic);

        std::memcpy(buffer.data() + offset, &m_version, sizeof(m_version));
        offset += sizeof(m_version);

        std::memcpy(buffer.data() + offset, &m_page_size, sizeof(m_page_size));
        offset += sizeof(m_page_size);

        std::memcpy(buffer.data() + offset, &m_page_count, sizeof(m_page_count));
        offset += sizeof(m_page_count);

        std::memcpy(buffer.data() + offset, &m_catalog_root, sizeof(m_catalog_root));
        offset += sizeof(m_catalog_root);

        std::memcpy(buffer.data() + offset, &m_clean_shutdown, sizeof(m_clean_shutdown));
        offset += sizeof(m_clean_shutdown);

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

        std::memcpy(&header.m_version, buffer.data() + offset, sizeof(header.m_version));
        offset += sizeof(header.m_version);

        std::memcpy(&header.m_page_size, buffer.data() + offset, sizeof(header.m_page_size));
        offset += sizeof(header.m_page_size);

        std::memcpy(&header.m_page_count, buffer.data() + offset, sizeof(header.m_page_count));
        offset += sizeof(header.m_page_count);

        std::memcpy(&header.m_catalog_root, buffer.data() + offset, sizeof(header.m_catalog_root));
        offset += sizeof(header.m_catalog_root);

        std::memcpy(&header.m_clean_shutdown, buffer.data() + offset, sizeof(header.m_clean_shutdown));
        offset += sizeof(header.m_clean_shutdown);

        return header;
    }

    bool FileHeader::valid() const
    {
        if (std::memcmp(magic, DATABASE_MAGIC, 4) != 0)
        {
            return false;
        }

        if (m_version != DATABASE_VERSION)
        {
            return false;
        }

        if (m_page_size != PAGE_SIZE)
        {
            return false;
        }

        return true;
    }
}
