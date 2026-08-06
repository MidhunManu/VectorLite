#pragma once

#include <memory>
#include <string>
#include "../src/storage/file_header.hpp"

namespace vectordb
{
    class Pager;

    class Database
    {
        public:
            static Database open(const std::string& path);
            ~Database();
            Database(Database&&) noexcept;
            Database& operator = (Database&&) noexcept;

            void close();

        private:
            Database(std::unique_ptr<Pager> pager, FileHeader header);
            std::unique_ptr<Pager> m_pager;
            FileHeader m_header;
    };
}
