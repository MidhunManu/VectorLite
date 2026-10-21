#include "query/planner/planner.hpp"
#include "query/query_error.hpp"
#include "catalog/catalog.hpp"
#include "catalog/collection_descriptor.hpp"

#include <limits>
#include <stdexcept>

namespace vectordb
{
    namespace
    {
        size_t column_index(const CollectionDescriptor& collection, const std::string& name)
        {
            for (size_t i = 0; i < collection.schema.size(); ++i)
            {
                if (collection.schema[i].name == name) return i;
            }
            throw QueryError("unknown column '" + name + "'", 0);
        }

        std::unique_ptr<LogicalNode> make_node(LogicalKind kind, std::unique_ptr<LogicalNode> child = nullptr)
        {
            auto node = std::make_unique<LogicalNode>();
            node->kind = kind;
            node->child = std::move(child);
            return node;
        }

        Value coerce(const LiteralValue& lit, const ColumnRef& col)
        {
            const int64_t* i = std::get_if<int64_t>(&lit);
            const double* d = std::get_if<double>(&lit);
            const std::string* s = std::get_if<std::string>(&lit);

            switch (col.type)
            {
                case ColumnType::Int32:
                    if (!i) break;
                    if (*i < std::numeric_limits<int32_t>::min() || *i > std::numeric_limits<int32_t>::max())
                        throw QueryError("value out of range for INT32 column '" + col.name + "'", 0);
                    return Value::from_int32(static_cast<int32_t>(*i));
                case ColumnType::Int64:
                    if (!i) break;
                    return Value::from_int64(*i);
                case ColumnType::Float32:
                    if (d) return Value::from_float32(static_cast<float>(*d));
                    if (i) return Value::from_float32(static_cast<float>(*i));
                    break;
                case ColumnType::String:
                    if (!s) break;
                    return Value::from_string(*s);
                case ColumnType::Bool:
                    if (!i) break;
                    if (*i != 0 && *i != 1)
                        throw QueryError("BOOL column '" + col.name + "' only accepts 0 or 1", 0);
                    return Value::from_bool(*i == 1);
                case ColumnType::Vector:
                    throw QueryError("vector columns are not supported until Milestone 6", 0);
            }
            throw QueryError("type mismatch for column '" + col.name + "'", 0);
        }
    }

    Planner::Planner(const Catalog& catalog) : m_catalog(catalog) {}

    const CollectionDescriptor& Planner::require(const std::string& name) const
    {
        const CollectionDescriptor* c = m_catalog.find_collection(name);
        if (!c) throw QueryError("unknown collection: " + name, 0);
        return *c;
    }

    LogicalPlan Planner::plan(const Statement& statement) const
    {
        if (auto* s = std::get_if<SelectStmt>(&statement)) return plan_select(*s);
        if (auto* s = std::get_if<InsertStmt>(&statement)) return plan_insert(*s);
        if (auto* s = std::get_if<DeleteStmt>(&statement)) return plan_delete(*s);
        if (auto* s = std::get_if<UpdateStmt>(&statement)) return plan_update(*s);
        throw std::logic_error("Planner::plan: statement kind is not plannable");
    }

    BoundPredicate Planner::bind_predicate(const Condition& cond, const CollectionDescriptor& collection) const
    {
        BoundPredicate bound;
        bound.connectors = cond.connectors;
        for (const Comparison& cmp : cond.comparisons)
        {
            size_t idx = column_index(collection, cmp.column.name);
            bound.comparisons.push_back(BoundComparison{idx, cmp.op, coerce(cmp.value, collection.schema[idx])});
        }
        return bound;
    }

    std::unique_ptr<LogicalNode> Planner::plan_source(
        const CollectionDescriptor& collection, const std::optional<Condition>& where) const
    {
        auto node = make_node(LogicalKind::Scan);
        node->collection = collection.name;

        if (where.has_value())
        {
            auto filter = make_node(LogicalKind::Filter, std::move(node));
            filter->predicate = bind_predicate(*where, collection);
            node = std::move(filter);
        }
        return node;
    }

    LogicalPlan Planner::plan_select(const SelectStmt& stmt) const
    {
        if (stmt.order_by.has_value())
        {
            throw QueryError("ORDER BY VECTOR_DISTANCE is not supported until Milestone 6", 0);
        }

        const CollectionDescriptor& collection = require(stmt.from);
        LogicalPlan plan;
        auto node = plan_source(collection, stmt.where);

        if (stmt.select_all)
        {
            for (const auto& col : collection.schema) plan.output_columns.push_back(col.name);
        }
        else
        {
            auto projection = make_node(LogicalKind::Projection, std::move(node));
            for (const auto& name : stmt.columns)
            {
                projection->projection.push_back(column_index(collection, name));
                plan.output_columns.push_back(name);
            }
            node = std::move(projection);
        }

        if (stmt.limit.has_value())
        {
            auto limit = make_node(LogicalKind::Limit, std::move(node));
            limit->limit = *stmt.limit;
            node = std::move(limit);
        }

        plan.root = std::move(node);
        return plan;
    }

    LogicalPlan Planner::plan_insert(const InsertStmt& stmt) const
    {
        const CollectionDescriptor& collection = require(stmt.collection_name);

        std::vector<Value> row(collection.schema.size(), Value::null());
        std::vector<bool> seen(collection.schema.size(), false);

        for (size_t i = 0; i < stmt.columns.size(); ++i)
        {
            size_t idx = column_index(collection, stmt.columns[i]);
            if (seen[idx]) throw QueryError("column '" + stmt.columns[i] + "' specified twice", 0);
            seen[idx] = true;

            const LiteralValue* lit = std::get_if<LiteralValue>(&stmt.values[i]);
            if (!lit)
            {
                throw QueryError("bind parameters ('?') are only used for vector columns (Milestone 6)", 0);
            }
            row[idx] = coerce(*lit, collection.schema[idx]);
        }

        for (size_t idx = 0; idx < seen.size(); ++idx)
        {
            if (!seen[idx] && !collection.schema[idx].nullable)
            {
                throw QueryError("column '" + collection.schema[idx].name + "' requires a value", 0);
            }
        }

        LogicalPlan plan;
        plan.root = make_node(LogicalKind::Insert);
        plan.root->collection = collection.name;
        plan.root->row = std::move(row);
        return plan;
    }

    LogicalPlan Planner::plan_delete(const DeleteStmt& stmt) const
    {
        const CollectionDescriptor& collection = require(stmt.from);

        LogicalPlan plan;
        plan.root = make_node(LogicalKind::Delete, plan_source(collection, stmt.where));
        plan.root->collection = collection.name;
        return plan;
    }

    LogicalPlan Planner::plan_update(const UpdateStmt& stmt) const
    {
        const CollectionDescriptor& collection = require(stmt.table);

        LogicalPlan plan;
        plan.root = make_node(LogicalKind::Update, plan_source(collection, stmt.where));
        plan.root->collection = collection.name;

        std::vector<bool> assigned(collection.schema.size(), false);
        for (const Assignment& a : stmt.assignments)
        {
            size_t idx = column_index(collection, a.column);
            if (assigned[idx]) throw QueryError("column '" + a.column + "' assigned twice", 0);
            assigned[idx] = true;

            const LiteralValue* lit = std::get_if<LiteralValue>(&a.value);
            if (!lit)
            {
                throw QueryError("bind parameters ('?') are only used for vector columns (Milestone 6)", 0);
            }
            plan.root->assignments.emplace_back(idx, coerce(*lit, collection.schema[idx]));
        }
        return plan;
    }
}
