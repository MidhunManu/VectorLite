#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "query/parser/ast.hpp"
#include "storage/record.hpp"

namespace vectordb
{
    struct BoundComparison { size_t column_index; ComparisonOp op; Value value; };

    struct BoundPredicate
    {
        std::vector<BoundComparison> comparisons;
        std::vector<LogicalOp> connectors;
    };

    enum class LogicalKind { Scan, Filter, Projection, Limit, Insert, Delete, Update };

    struct LogicalNode
    {
        LogicalKind kind = LogicalKind::Scan;
        std::unique_ptr<LogicalNode> child;

        std::string collection;                            // Scan/Insert/Delete/Update
        BoundPredicate predicate;                          // Filter
        std::vector<size_t> projection;                    // Projection
        int64_t limit = 0;                                 // Limit
        std::vector<Value> row;                            // Insert: full row, schema order
        std::vector<std::pair<size_t, Value>> assignments; // Update
    };

    struct LogicalPlan
    {
        std::unique_ptr<LogicalNode> root;
        std::vector<std::string> output_columns;
    };
}
