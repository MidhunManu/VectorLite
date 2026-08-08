#include "vectordb/database.hpp"
#include "../storage/pager.hpp"
#include "../src/storage/file_header.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace vectordb
{
    class DatabaseImpl
    {
        public:
            std::unique_ptr<Pager> pager;
            FileHeader header;
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
        auto pager = std::make_unique<Pager>(path);

        FileHeader header;

        if (!exists)
        {
            impl->header = FileHeader::create();
            auto buffer = impl->header.serialise();
            pager->write_page(0, buffer);
        }
        else
        {
            auto buffer = impl->pager->read_page(0);
            impl->header = FileHeader::deserialise(buffer);

            if (!impl->header.valid())
            {
                throw std::runtime_error("corrupt or incompatible database file: " + path);
            }
        }

        return Database(std::move(impl));
    }

    void Database::close()
    {
        m_impl.reset();
    }
}
