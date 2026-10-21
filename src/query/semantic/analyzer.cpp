#include "query/semantic/analyzer.hpp"
#include "query/query_error.hpp"
#include "catalog/catalog.hpp"
#include "catalog/collection_descriptor.hpp"
#include "catalog/schema.hpp"

#include <type_traits>
#include <variant>

namespace vectordb
{
    SemanticAnalyzer::SemanticAnalyzer(const Catalog& catalog) : m_catalog(catalog) {}

    const CollectionDescriptor& SemanticAnalyzer::require_collection(const std::string& name) const
    {
        const CollectionDescriptor* c = m_catalog.find_collection(name);
        if (!c) throw QueryError("unknown collection: " + name, 0);
        return *c;
    }

    const ColumnDef& SemanticAnalyzer::require_column(
        const CollectionDescriptor& collection, const std::string& name) const
    {
        for (const auto& col : collection.schema)
        {
            if (col.name == name) return col;
        }
        throw QueryError("unknown column '" + name + "' in collection '" + collection.name + "'", 0);
    }

    void SemanticAnalyzer::check_literal_type(const LiteralValue& value, const ColumnDef& column) const
    {
        bool ok = false;
        switch (column.type)
        {
            case ColumnType::Int32:
            case ColumnType::Int64:
                ok = std::holds_alternative<int64_t>(value);
                break;
            case ColumnType::Float32:
                ok = std::holds_alternative<double>(value) || std::holds_alternative<int64_t>(value);
                break;
            case ColumnType::String:
                ok = std::holds_alternative<std::string>(value);
                break;
            case ColumnType::Bool:
                // No boolean literal in the grammar (spec Section 6.2) --
                // MVP convention: 0/1 integer literal coerces to bool.
                ok = std::holds_alternative<int64_t>(value);
                break;
            case ColumnType::Vector:
                ok = false; // vectors are never literals -- always bound via '?'
                break;
        }
        if (!ok)
        {
            throw QueryError("type mismatch for column '" + column.name + "'", 0);
        }
    }

    void SemanticAnalyzer::check_condition(const Condition& cond, const CollectionDescriptor& collection) const
    {
        for (const auto& cmp : cond.comparisons)
        {
            const ColumnDef& col = require_column(collection, cmp.column.name);
            check_literal_type(cmp.value, col);
        }
    }

    void SemanticAnalyzer::analyze_create(const CreateCollectionStmt& stmt) const
    {
        if (m_catalog.find_collection(stmt.collection_name) != nullptr)
        {
            throw QueryError("collection already exists: " + stmt.collection_name, 0);
        }

        for (const auto& col : stmt.columns)
        {
            auto resolved = parse_column_type(col.type_name);
            if (!resolved.has_value())
            {
                throw QueryError("unknown column type: " + col.type_name, 0);
            }
            if (col.vector_dim.has_value() && *resolved != ColumnType::Float32)
            {
                throw QueryError(
                    "VECTOR(...) columns must declare element type FLOAT32, got " + col.type_name, 0);
            }
        }
    }

    void SemanticAnalyzer::analyze_insert(const InsertStmt& stmt) const
    {
        const CollectionDescriptor& collection = require_collection(stmt.collection_name);

        for (size_t i = 0; i < stmt.columns.size(); ++i)
        {
            const ColumnDef& col = require_column(collection, stmt.columns[i]);
            const ValueExpr& expr = stmt.values[i];

            if (std::holds_alternative<BindParam>(expr))
            {
                if (col.type != ColumnType::Vector)
                {
                    throw QueryError("column '" + col.name + "' is not VECTOR, cannot bind '?' to it", 0);
                }
                continue; // dimension checked at bind time, spec Section 8.2
            }

            if (col.type == ColumnType::Vector)
            {
                throw QueryError("vector column '" + col.name + "' must be supplied via bound '?'", 0);
            }

            check_literal_type(std::get<LiteralValue>(expr), col);
        }
    }

    void SemanticAnalyzer::analyze_select(const SelectStmt& stmt) const
    {
        const CollectionDescriptor& collection = require_collection(stmt.from);

        if (!stmt.select_all)
        {
            for (const auto& col_name : stmt.columns)
            {
                require_column(collection, col_name);
            }
        }

        if (stmt.where.has_value())
        {
            check_condition(*stmt.where, collection);
        }

        if (stmt.order_by.has_value())
        {
            const ColumnDef& col = require_column(collection, stmt.order_by->column);
            if (col.type != ColumnType::Vector)
            {
                throw QueryError("VECTOR_DISTANCE requires a VECTOR column, '" + col.name + "' is not one", 0);
            }
        }
    }

    void SemanticAnalyzer::analyze_delete(const DeleteStmt& stmt) const
    {
        const CollectionDescriptor& collection = require_collection(stmt.from);
        if (stmt.where.has_value())
        {
            check_condition(*stmt.where, collection);
        }
    }

    void SemanticAnalyzer::analyze_update(const UpdateStmt& stmt) const
    {
        const CollectionDescriptor& collection = require_collection(stmt.table);

        for (const auto& assignment : stmt.assignments)
        {
            const ColumnDef& col = require_column(collection, assignment.column);

            if (std::holds_alternative<BindParam>(assignment.value))
            {
                if (col.type != ColumnType::Vector)
                {
                    throw QueryError("column '" + col.name + "' is not VECTOR, cannot bind '?' to it", 0);
                }
                continue;
            }

            if (col.type == ColumnType::Vector)
            {
                throw QueryError("vector column '" + col.name + "' must be supplied via bound '?'", 0);
            }

            check_literal_type(std::get<LiteralValue>(assignment.value), col);
        }

        if (stmt.where.has_value())
        {
            check_condition(*stmt.where, collection);
        }
    }

    void SemanticAnalyzer::analyze(const Statement& statement) const
    {
        std::visit([this](const auto& stmt)
        {
            using T = std::decay_t<decltype(stmt)>;
            if constexpr (std::is_same_v<T, CreateCollectionStmt>) analyze_create(stmt);
            else if constexpr (std::is_same_v<T, InsertStmt>) analyze_insert(stmt);
            else if constexpr (std::is_same_v<T, SelectStmt>) analyze_select(stmt);
            else if constexpr (std::is_same_v<T, DeleteStmt>) analyze_delete(stmt);
            else if constexpr (std::is_same_v<T, UpdateStmt>) analyze_update(stmt);
        }, statement);
    }
}
