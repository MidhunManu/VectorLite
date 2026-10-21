#include "exec/write_ops.hpp"
#include "catalog/collection_descriptor.hpp"
#include "storage/storage_engine.hpp"

namespace vectordb
{
    InsertOp::InsertOp(ExecContext& ctx, CollectionDescriptor& collection, Record record)
        : m_ctx(ctx), m_collection(collection), m_record(std::move(record)) {}

    uint64_t InsertOp::run()
    {
        std::vector<char> bytes = m_record.serialise();
        uint32_t root_before = m_collection.root_page;

        RecordId id = m_ctx.storage.insert_record(m_collection, bytes);
        m_ctx.sink.log_insert(m_collection.id, id, std::move(bytes));

        if (m_collection.root_page != root_before) m_ctx.on_catalog_changed();
        return 1;
    }

    DeleteOp::DeleteOp(ExecContext& ctx, CollectionDescriptor& collection, std::unique_ptr<Operator> child)
        : m_ctx(ctx), m_collection(collection), m_child(std::move(child)) {}

    uint64_t DeleteOp::run()
    {
        std::vector<Row> targets = drain(*m_child);

        uint64_t deleted = 0;
        for (const Row& row : targets)
        {
            if (m_ctx.storage.delete_record(row.id))
            {
                m_ctx.sink.log_delete(m_collection.id, row.id);
                ++deleted;
            }
        }
        return deleted;
    }

    UpdateOp::UpdateOp(ExecContext& ctx, CollectionDescriptor& collection, std::unique_ptr<Operator> child,
                       std::vector<std::pair<size_t, Value>> assignments)
        : m_ctx(ctx), m_collection(collection), m_child(std::move(child)), m_assignments(std::move(assignments)) {}

    uint64_t UpdateOp::run()
    {
        std::vector<Row> targets = drain(*m_child);

        uint64_t updated = 0;
        for (const Row& row : targets)
        {
            std::vector<Value> values = row.record.values();
            for (const auto& [index, value] : m_assignments) values.at(index) = value;

            std::vector<char> bytes = Record(std::move(values)).serialise();
            RecordId new_id = m_ctx.storage.insert_record(m_collection, bytes);
            m_ctx.sink.log_insert(m_collection.id, new_id, std::move(bytes));

            if (m_ctx.storage.delete_record(row.id))
            {
                m_ctx.sink.log_delete(m_collection.id, row.id);
            }
            ++updated;
        }
        return updated;
    }
}
