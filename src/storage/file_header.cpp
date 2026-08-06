#include "file_header.hpp"
#include "vectordb/config.hpp"
#include "cstring"
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
}
