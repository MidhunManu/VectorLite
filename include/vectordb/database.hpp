#pragma once

#include <memory>
#include <string>
#include <vector>

namespace vectordb
{
    class Pager;
    class DatabaseImpl;
    class CollectionDescriptor;
    struct ColumnRef;

    class Database
    {
        public:
            static Database open(const std::string& path);
            ~Database();
            Database(Database&&) noexcept;
            Database& operator = (Database&&) noexcept;

            void close();

            const CollectionDescriptor& create_collection(
                const std::string& name,
                std::vector<ColumnRef> schema
            );
            const CollectionDescriptor* find_collection(const std::string& name) const;

        private:
            explicit Database(std::unique_ptr<DatabaseImpl> impl);
            void persist_catalog();
            std::unique_ptr<DatabaseImpl> m_impl;
    };
}
