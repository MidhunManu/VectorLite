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

        uint32_t catalog_root() const
        {
            return m_catalog_root;
        }

        void set_catalog_root(uint64_t page_id) {
            m_catalog_root = page_id;
        }

        uint64_t page_count() const { return m_page_count; }
        void set_page_count(uint64_t count) { m_page_count = count; }

        uint8_t clean_shutdown() const { return m_clean_shutdown; }
        void set_clean_shutdown(uint8_t value) { m_clean_shutdown = value; }

    private:
        char magic[4];
        uint32_t m_version;
        uint32_t m_page_size;
        uint64_t m_page_count;
        uint64_t m_catalog_root;
        uint8_t m_clean_shutdown;
    };
}
