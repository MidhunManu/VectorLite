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
#include "query/parser/ast.hpp"
#include "vectordb/result.hpp"

namespace vectordb
{
    class Pager;
    class BufferManager;
    class StorageEngine;
    class WalWriter;
    class Transaction;

    class Database
    {
    public:
        static Database open(const std::string &path);

        ~Database();
        Database(Database &&) noexcept;
        Database &operator=(Database &&) noexcept;

        // Checkpoint: flush pages, fsync, truncate WAL. Any open explicit transaction is rolled back.
        void close();

        // SQL entry point. Statements outside BEGIN/COMMIT auto-commit.
        ResultSet execute(const std::string &sql);

        // Catalog API (M2).
        const CollectionDescriptor &create_collection(const std::string &name, std::vector<ColumnRef> schema);
        const CollectionDescriptor *find_collection(const std::string &name) const;

        // Low-level row API (M3). Bypasses transactions and the WAL; kept for storage tests.
        RecordId insert_record(const std::string &collection_name, const Record &record);
        std::optional<Record> read_record(const std::string &collection_name, RecordId id);
        bool delete_record(const std::string &collection_name, RecordId id);
        std::vector<std::pair<RecordId, Record>> scan_collection(const std::string &collection_name);

    private:
        Database(
            std::unique_ptr<Pager> pager,
            std::unique_ptr<FileHeader> header,
            std::unique_ptr<BufferManager> buffer_manager,
            std::unique_ptr<StorageEngine> storage_engine,
            std::unique_ptr<WalWriter> wal,
            Catalog catalog);

        void persist_catalog();
        void require_open() const;
        CollectionDescriptor &require_collection(const std::string &name);

        ResultSet run_dml(const Statement &statement);
        ResultSet run_create(const CreateCollectionStmt &stmt);
        void begin_transaction();
        void commit_active();
        void rollback_active();

        std::unique_ptr<Pager> m_pager;
        std::unique_ptr<FileHeader> m_header;
        std::unique_ptr<BufferManager> m_buffer_manager;
        std::unique_ptr<StorageEngine> m_storage_engine;
        std::unique_ptr<WalWriter> m_wal;
        Catalog m_catalog;
        std::unique_ptr<Transaction> m_txn; // non-null while a transaction is open
        uint64_t m_next_txn_id = 1;
    };
}
