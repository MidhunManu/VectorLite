#pragma once

#include <memory>
#include <string>

namespace vectordb
{
    class Pager;
    class DatabaseImpl;

    class Database
    {
        public:
            static Database open(const std::string& path);
            ~Database();
            Database(Database&&) noexcept;
            Database& operator = (Database&&) noexcept;

            void close();

        private:
            explicit Database(std::unique_ptr<DatabaseImpl> impl);
            std::unique_ptr<DatabaseImpl> m_impl;
    };
}
