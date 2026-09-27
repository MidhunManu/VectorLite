#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "storage/file_header.hpp"
#include "storage/slotted_page.hpp"
#include "storage/record.hpp"
#include "catalog/catalog.hpp"
#include "catalog/schema.hpp"

namespace vectordb
{
    class Pager;
    class BufferManager;
    class StorageEngine;

    class Database
    {
    public:
        static Database open(const std::string& path);

        ~Database();
        Database(Database&&) noexcept;
        Database& operator=(Database&&) noexcept;

        void close();

        const CollectionDescriptor& create_collection(
            const std::string& name,
            std::vector<ColumnRef> schema);

        const CollectionDescriptor* find_collection(const std::string& name) const;

        RecordId insert_record(const std::string& collection_name, const Record& record);
        std::optional<Record> read_record(const std::string& collection_name, RecordId id);
        bool delete_record(const std::string& collection_name, RecordId id);
        std::vector<std::pair<RecordId, Record>> scan_collection(const std::string& collection_name);

    private:
        Database(
            std::unique_ptr<Pager> pager,
            std::unique_ptr<FileHeader> header,
            std::unique_ptr<BufferManager> buffer_manager,
            std::unique_ptr<StorageEngine> storage_engine,
            Catalog catalog);

        void persist_catalog();
        CollectionDescriptor& require_collection(const std::string& name);

        std::unique_ptr<Pager> m_pager;
        std::unique_ptr<FileHeader> m_header;
        std::unique_ptr<BufferManager> m_buffer_manager;
        std::unique_ptr<StorageEngine> m_storage_engine;
        Catalog m_catalog;
    };
}