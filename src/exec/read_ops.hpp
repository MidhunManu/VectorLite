#pragma once
#include <memory>
#include <vector>
#include "exec/operator.hpp"
#include "query/planner/logical_plan.hpp"

namespace vectordb
{
    class StorageEngine;
    struct CollectionDescriptor;

    class CollectionScanOp : public Operator
    {
    public:
        CollectionScanOp(StorageEngine& storage, const CollectionDescriptor& collection);
        void open() override;
        std::optional<Row> next() override;
        void close() override;

    private:
        StorageEngine& m_storage;
        const CollectionDescriptor& m_collection;
        std::vector<std::pair<RecordId, std::vector<char>>> m_rows;
        size_t m_index = 0;
    };

    class MetadataFilterOp : public Operator
    {
    public:
        MetadataFilterOp(std::unique_ptr<Operator> child, BoundPredicate predicate);
        void open() override;
        std::optional<Row> next() override;
        void close() override;

    private:
        std::unique_ptr<Operator> m_child;
        BoundPredicate m_predicate;
    };

    class ProjectionOp : public Operator
    {
    public:
        ProjectionOp(std::unique_ptr<Operator> child, std::vector<size_t> columns);
        void open() override;
        std::optional<Row> next() override;
        void close() override;

    private:
        std::unique_ptr<Operator> m_child;
        std::vector<size_t> m_columns;
    };

    class LimitOp : public Operator
    {
    public:
        LimitOp(std::unique_ptr<Operator> child, uint64_t limit);
        void open() override;
        std::optional<Row> next() override;
        void close() override;

    private:
        std::unique_ptr<Operator> m_child;
        uint64_t m_limit;
        uint64_t m_count = 0;
    };
}
