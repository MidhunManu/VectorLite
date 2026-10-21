#pragma once
#include <memory>
#include <utility>
#include <vector>
#include "exec/exec_context.hpp"
#include "exec/operator.hpp"

namespace vectordb
{
    struct CollectionDescriptor;

    class InsertOp : public WriteOperator
    {
    public:
        InsertOp(ExecContext& ctx, CollectionDescriptor& collection, Record record);
        uint64_t run() override;

    private:
        ExecContext& m_ctx;
        CollectionDescriptor& m_collection;
        Record m_record;
    };

    class DeleteOp : public WriteOperator
    {
    public:
        DeleteOp(ExecContext& ctx, CollectionDescriptor& collection, std::unique_ptr<Operator> child);
        uint64_t run() override;

    private:
        ExecContext& m_ctx;
        CollectionDescriptor& m_collection;
        std::unique_ptr<Operator> m_child;
    };

    class UpdateOp : public WriteOperator
    {
    public:
        UpdateOp(ExecContext& ctx, CollectionDescriptor& collection, std::unique_ptr<Operator> child,
                 std::vector<std::pair<size_t, Value>> assignments);
        uint64_t run() override;

    private:
        ExecContext& m_ctx;
        CollectionDescriptor& m_collection;
        std::unique_ptr<Operator> m_child;
        std::vector<std::pair<size_t, Value>> m_assignments;
    };
}
