#include "vectordb/database.hpp"
#include "storage/pager.hpp"
#include "storage/buffer_manager.hpp"
#include "storage/storage_engine.hpp"
#include "storage/file_header.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace vectordb
{
    namespace
    {
        constexpr size_t BUFFER_POOL_CAPACITY = 64;
    }

    Database::Database(
        std::unique_ptr<Pager> pager,
        std::unique_ptr<FileHeader> header,
        std::unique_ptr<BufferManager> buffer_manager,
        std::unique_ptr<StorageEngine> storage_engine,
        Catalog catalog)
        : m_pager(std::move(pager))
        , m_header(std::move(header))
        , m_buffer_manager(std::move(buffer_manager))
        , m_storage_engine(std::move(storage_engine))
        , m_catalog(std::move(catalog))
    {
    }

    Database::~Database() = default;
    Database::Database(Database&&) noexcept = default;
    Database& Database::operator=(Database&&) noexcept = default;

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
        auto header = std::make_unique<FileHeader>();
        Catalog catalog;

        if (!exists)
        {
            *header = FileHeader::create();
            header->set_page_count(2); // page 0 = header, page 1 = catalog root
            header->set_catalog_root(1);

            auto header_buffer = header->serialise();
            pager->write_page(0, header_buffer);

            auto catalog_buffer = catalog.serialise();
            pager->write_page(header->catalog_root(), catalog_buffer);
        }
        else
        {
            auto header_buffer = pager->read_page(0);
            *header = FileHeader::deserialise(header_buffer);

            if (!header->valid())
            {
                throw std::runtime_error("corrupt or incompatible database file: " + path);
            }

            auto catalog_buffer = pager->read_page(header->catalog_root());
            catalog = Catalog::deserialise(catalog_buffer);
        }

        auto buffer_manager = std::make_unique<BufferManager>(*pager, BUFFER_POOL_CAPACITY);
        auto storage_engine = std::make_unique<StorageEngine>(*buffer_manager, *pager, *header);

        return Database(
            std::move(pager),
            std::move(header),
            std::move(buffer_manager),
            std::move(storage_engine),
            std::move(catalog));
    }

    void Database::close()
    {
        if (m_buffer_manager)
        {
            m_buffer_manager->flush_all();
        }
        m_pager.reset();
    }

    const CollectionDescriptor& Database::create_collection(
        const std::string& name,
        std::vector<ColumnRef> schema)
    {
        const CollectionDescriptor& descriptor =
            m_catalog.create_collection(name, std::move(schema));

        persist_catalog();

        return descriptor;
    }

    const CollectionDescriptor* Database::find_collection(const std::string& name) const
    {
        return m_catalog.find_collection(name);
    }

    CollectionDescriptor& Database::require_collection(const std::string& name)
    {
        CollectionDescriptor* descriptor = m_catalog.find_collection(name);
        if (descriptor == nullptr)
        {
            throw std::runtime_error("no such collection: " + name);
        }
        return *descriptor;
    }

    RecordId Database::insert_record(const std::string& collection_name, const Record& record)
    {
        CollectionDescriptor& collection = require_collection(collection_name);
        uint32_t root_before = collection.root_page;

        RecordId id = m_storage_engine->insert_record(collection, record.serialise());

        if (collection.root_page != root_before)
        {
            persist_catalog(); // first record allocated the root page
        }

        return id;
    }

    std::optional<Record> Database::read_record(const std::string& collection_name, RecordId id)
    {
        require_collection(collection_name);

        auto bytes = m_storage_engine->read_record(id);
        if (!bytes.has_value())
        {
            return std::nullopt;
        }
        return Record::deserialise(*bytes);
    }

    bool Database::delete_record(const std::string& collection_name, RecordId id)
    {
        require_collection(collection_name);
        return m_storage_engine->delete_record(id);
    }

    std::vector<std::pair<RecordId, Record>> Database::scan_collection(const std::string& collection_name)
    {
        const CollectionDescriptor& collection = require_collection(collection_name);

        auto raw = m_storage_engine->scan_collection(collection);

        std::vector<std::pair<RecordId, Record>> results;
        results.reserve(raw.size());
        for (auto& [id, bytes] : raw)
        {
            results.emplace_back(id, Record::deserialise(bytes));
        }
        return results;
    }

    void Database::persist_catalog()
    {
        auto buffer = m_catalog.serialise();
        m_pager->write_page(m_header->catalog_root(), buffer);
    }
}