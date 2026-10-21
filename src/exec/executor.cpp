#include "exec/executor.hpp"
#include "exec/read_ops.hpp"
#include "exec/write_ops.hpp"
#include "catalog/catalog.hpp"
#include "catalog/collection_descriptor.hpp"

#include <stdexcept>

namespace vectordb
{
    Executor::Executor(Catalog& catalog, ExecContext context)
        : m_catalog(catalog), m_ctx(std::move(context)) {}

    CollectionDescriptor& Executor::require(const std::string& name)
    {
        CollectionDescriptor* c = m_catalog.find_collection(name);
        if (!c) throw std::runtime_error("no such collection: " + name);
        return *c;
    }

    std::unique_ptr<Operator> Executor::build_read_tree(const LogicalNode& node)
    {
        switch (node.kind)
        {
            case LogicalKind::Scan:
                return std::make_unique<CollectionScanOp>(m_ctx.storage, require(node.collection));
            case LogicalKind::Filter:
                return std::make_unique<MetadataFilterOp>(build_read_tree(*node.child), node.predicate);
            case LogicalKind::Projection:
                return std::make_unique<ProjectionOp>(build_read_tree(*node.child), node.projection);
            case LogicalKind::Limit:
                return std::make_unique<LimitOp>(build_read_tree(*node.child), static_cast<uint64_t>(node.limit));
            default:
                throw std::logic_error("build_read_tree: not a read operator");
        }
    }

    ResultSet Executor::execute(const LogicalPlan& plan)
    {
        const LogicalNode& root = *plan.root;
        ResultSet result;

        switch (root.kind)
        {
            case LogicalKind::Insert:
            {
                InsertOp op(m_ctx, require(root.collection), Record(root.row));
                result.affected_rows = op.run();
                break;
            }
            case LogicalKind::Delete:
            {
                DeleteOp op(m_ctx, require(root.collection), build_read_tree(*root.child));
                result.affected_rows = op.run();
                break;
            }
            case LogicalKind::Update:
            {
                UpdateOp op(m_ctx, require(root.collection), build_read_tree(*root.child), root.assignments);
                result.affected_rows = op.run();
                break;
            }
            default:
            {
                auto tree = build_read_tree(root);
                result.columns = plan.output_columns;
                for (const Row& row : drain(*tree)) result.rows.push_back(row.record.values());
                break;
            }
        }
        return result;
    }
}
