#pragma once
#include <cstdint>
#include <vector>
#include "exec/exec_context.hpp"
#include "wal/wal_record.hpp"

namespace vectordb
{
    class StorageEngine;

    // Single-writer, non-nested (spec 4.2). Mutations are applied to buffer-pool
    // pages immediately (so the transaction reads its own writes) and logged here;
    // nothing reaches the WAL or the main file until Database commits.
    class Transaction : public MutationSink
    {
    public:
        explicit Transaction(uint64_t id) : m_id(id) {}

        uint64_t id() const { return m_id; }
        const std::vector<WalOp>& ops() const { return m_ops; }

        void log_insert(uint32_t collection_id, const RecordId& id, std::vector<char> bytes) override;
        void log_delete(uint32_t collection_id, const RecordId& id) override;

        // Statement-level savepoint: undo everything logged after `mark`.
        size_t mark() const { return m_ops.size(); }
        void rollback_to(size_t mark, StorageEngine& storage);

    private:
        uint64_t m_id;
        std::vector<WalOp> m_ops;
    };
}
