#include "vectordb/database.hpp"
#include "../storage/pager.hpp"
#include "../src/storage/file_header.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>


namespace vectordb
{
    Database::Database(std::unique_ptr<Pager> pager, FileHeader header)
        : m_pager(std::move(pager)), m_header(header)
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

        auto pager = std::make_unique<Pager>(path);

        FileHeader header;

        if (!exists)
        {
            header = FileHeader::create();
            auto buffer = header.serialise();
            pager->write_page(0, buffer);
        }
        else
        {
            auto buffer = pager->read_page(0);
            header = FileHeader::deserialise(buffer);

            if (!header.valid())
            {
                throw std::runtime_error("corrupt or incompatible database file: " + path);
            }
        }

        return Database(std::move(pager), header);
    }

    void Database::close()
    {
        m_pager.reset();
    }
}
