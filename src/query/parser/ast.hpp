#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace vectordb
{
    enum class ComparisonOp
    {
        Eq,
        Ne,
        Lt,
        Gt,
        Le,
        Ge
    };
    enum class LogicalOp
    {
        And,
        Or
    };

    // Deliberately separate from storage::Value -- keeps the Query layer
    using LiteralValue = std::variant<int64_t, double, std::string, bool>;

    struct ColumnRef
    {
        std::string name;
    };
    struct BindParam
    {
        uint32_t index;
    }; // '?' -- positional, 0-based
    using ValueExpr = std::variant<LiteralValue, BindParam>;

    struct Comparison
    {
        ColumnRef column;
        ComparisonOp op;
        LiteralValue value;
    };

    struct Condition
    {
        std::vector<Comparison> comparisons;
        std::vector<LogicalOp> connectors; // size == comparisons.size() - 1
    };

    struct VectorDistanceExpr
    {
        std::string column;
        BindParam param;
    };

    struct ColumnDefNode
    {
        std::string name;
        std::string type_name;
        std::optional<uint32_t> vector_dim;
    };

    struct CreateCollectionStmt
    {
        std::string collection_name;
        std::vector<ColumnDefNode> columns;
    };

    struct InsertStmt
    {
        std::string collection_name;
        std::vector<std::string> columns;
        std::vector<ValueExpr> values;
    };

    struct SelectStmt
    {
        bool select_all = true;
        std::vector<std::string> columns;
        std::string from;
        std::optional<Condition> where;
        std::optional<VectorDistanceExpr> order_by;
        std::optional<int64_t> limit;
    };

    struct DeleteStmt
    {
        std::string from;
        std::optional<Condition> where;
    };

    struct Assignment
    {
        std::string column;
        ValueExpr value;
    };

    struct UpdateStmt
    {
        std::string table;
        std::vector<Assignment> assignments;
        std::optional<Condition> where;
    };

    struct BeginStmt
    {
    };
    struct CommitStmt
    {
    };
    struct RollbackStmt
    {
    };

    using Statement = std::variant<
        CreateCollectionStmt, InsertStmt, SelectStmt, DeleteStmt,
        UpdateStmt, BeginStmt, CommitStmt, RollbackStmt>;
}
