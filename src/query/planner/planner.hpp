#pragma once
#include "query/parser/ast.hpp"
#include "query/planner/logical_plan.hpp"

namespace vectordb
{
    class Catalog;
    struct CollectionDescriptor;

    // Plans SELECT/INSERT/UPDATE/DELETE. DDL and BEGIN/COMMIT/ROLLBACK are
    // handled by Database, not planned.
    class Planner
    {
    public:
        explicit Planner(const Catalog& catalog);
        LogicalPlan plan(const Statement& statement) const;

    private:
        const Catalog& m_catalog;

        LogicalPlan plan_select(const SelectStmt& stmt) const;
        LogicalPlan plan_insert(const InsertStmt& stmt) const;
        LogicalPlan plan_delete(const DeleteStmt& stmt) const;
        LogicalPlan plan_update(const UpdateStmt& stmt) const;

        std::unique_ptr<LogicalNode> plan_source(
            const CollectionDescriptor& collection, const std::optional<Condition>& where) const;
        BoundPredicate bind_predicate(const Condition& cond, const CollectionDescriptor& collection) const;
        const CollectionDescriptor& require(const std::string& name) const;
    };
}
