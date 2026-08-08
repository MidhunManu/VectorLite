#include "vectordb/database.hpp"
#include "../storage/pager.hpp"
#include "../src/storage/file_header.hpp"
#include "../src/catalog/collection_descriptor.hpp"
#include "catalog/catalog.hpp"
#include "../src/catalog/schema.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace vectordb
{
    class DatabaseImpl
    {
        public:
            std::unique_ptr<Pager> pager;
            FileHeader header;
            Catalog catalog;
    };

    Database::Database(std::unique_ptr<DatabaseImpl> impl)
        : m_impl(std::move(impl))
    {
    }

    Database::~Database() = default;
    Database::Database(Database&&) noexcept = default;
    Database& Database::operator = (Database&&) noexcept = default;

    Database Database::open(const std::string& path)
    {
        bool exists = std::filesystem::exists(path);

        if (!exists)
        {
            std::ofstream create_file(path, std::ios::binary);
            if (!create_file)
            {
                throw std::runtime_error("cannot create database file: " + path);
            }
        }

        auto impl = std::make_unique<DatabaseImpl>();
        impl->pager = std::make_unique<Pager>(path);

        FileHeader header;

        if (!exists)
        {
            impl->header = FileHeader::create();
            impl->header.set_catalog_root(1);
            impl->header.set_page_count(2);

            auto header_buffer = impl->header.serialise();
            impl->pager->write_page(0, header_buffer);

            auto catalog_buffer = impl->catalog.serialise(); // empty catalog
            impl->pager->write_page(impl->header.catalog_root(), catalog_buffer);
        }
        else
        {
            auto buffer = impl->pager->read_page(0);
            impl->header = FileHeader::deserialise(buffer);

            if (!impl->header.valid())
            {
                throw std::runtime_error("corrupt or incompatible database file: " + path);
            }

            auto catalog_buffer = impl->pager->read_page(impl->header.catalog_root());
            impl->catalog = Catalog::deserialise(catalog_buffer);
        }

        return Database(std::move(impl));
    }

    void Database::close()
    {
        m_impl.reset();
    }

    const CollectionDescriptor& Database::create_collection(
        const std::string& name,
        std::vector<ColumnRef> schema
    )
    {
        const CollectionDescriptor& descriptor = m_impl->catalog.create_collection(name, std::move(schema));

        persist_catalog();

        return descriptor;
    }

    const CollectionDescriptor* Database::find_collection(const std::string& name) const
    {
        return m_impl->catalog.find_collection(name);
    }


    void Database::persist_catalog()
    {
        auto buffer = m_impl->catalog.serialise();
        m_impl->pager->write_page(m_impl->header.catalog_root(), buffer);
    }
}
