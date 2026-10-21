#include "exec/read_ops.hpp"
#include "exec/eval.hpp"
#include "storage/storage_engine.hpp"

namespace vectordb
{
    CollectionScanOp::CollectionScanOp(StorageEngine& storage, const CollectionDescriptor& collection)
        : m_storage(storage), m_collection(collection) {}

    void CollectionScanOp::open()
    {
        m_rows = m_storage.scan_collection(m_collection);
        m_index = 0;
    }

    std::optional<Row> CollectionScanOp::next()
    {
        if (m_index >= m_rows.size()) return std::nullopt;
        auto& [id, bytes] = m_rows[m_index++];
        return Row{id, Record::deserialise(bytes)};
    }

    void CollectionScanOp::close()
    {
        m_rows.clear();
        m_index = 0;
    }

    MetadataFilterOp::MetadataFilterOp(std::unique_ptr<Operator> child, BoundPredicate predicate)
        : m_child(std::move(child)), m_predicate(std::move(predicate)) {}

    void MetadataFilterOp::open() { m_child->open(); }

    std::optional<Row> MetadataFilterOp::next()
    {
        while (auto row = m_child->next())   // terminates: child is finite
        {
            if (evaluate_predicate(m_predicate, row->record)) return row;
        }
        return std::nullopt;
    }

    void MetadataFilterOp::close() { m_child->close(); }

    ProjectionOp::ProjectionOp(std::unique_ptr<Operator> child, std::vector<size_t> columns)
        : m_child(std::move(child)), m_columns(std::move(columns)) {}

    void ProjectionOp::open() { m_child->open(); }

    std::optional<Row> ProjectionOp::next()
    {
        auto row = m_child->next();
        if (!row.has_value()) return std::nullopt;

        std::vector<Value> values;
        values.reserve(m_columns.size());
        for (size_t idx : m_columns) values.push_back(row->record.at(idx));
        return Row{row->id, Record(std::move(values))};
    }

    void ProjectionOp::close() { m_child->close(); }

    LimitOp::LimitOp(std::unique_ptr<Operator> child, uint64_t limit)
        : m_child(std::move(child)), m_limit(limit) {}

    void LimitOp::open()
    {
        m_count = 0;
        m_child->open();
    }

    std::optional<Row> LimitOp::next()
    {
        if (m_count >= m_limit) return std::nullopt;
        auto row = m_child->next();
        if (row.has_value()) ++m_count;
        return row;
    }

    void LimitOp::close() { m_child->close(); }
}
