#pragma once
#include <memory>
#include "exec/exec_context.hpp"
#include "exec/operator.hpp"
#include "query/planner/logical_plan.hpp"
#include "vectordb/result.hpp"

namespace vectordb
{
    class Catalog;
    struct CollectionDescriptor;

    class Executor
    {
    public:
        Executor(Catalog& catalog, ExecContext context);
        ResultSet execute(const LogicalPlan& plan);

    private:
        Catalog& m_catalog;
        ExecContext m_ctx;

        std::unique_ptr<Operator> build_read_tree(const LogicalNode& node);
        CollectionDescriptor& require(const std::string& name);
    };
}
