#pragma once

#include "catalog/schema.hpp"
#include "query/parser/ast.hpp"

namespace vectordb
{
    class Catalog;
    struct CollectionDescriptor;

    class SemanticAnalyzer
    {
    public:
        explicit SemanticAnalyzer(const Catalog& catalog);
        void analyze(const Statement& statement) const;

    private:
        const Catalog& m_catalog;

        void analyze_create(const CreateCollectionStmt& stmt) const;
        void analyze_insert(const InsertStmt& stmt) const;
        void analyze_select(const SelectStmt& stmt) const;
        void analyze_delete(const DeleteStmt& stmt) const;
        void analyze_update(const UpdateStmt& stmt) const;

        void check_condition(const Condition& cond, const CollectionDescriptor& collection) const;
        void check_literal_type(const LiteralValue& value, const ColumnRef& column) const;
        const ColumnRef& require_column(const CollectionDescriptor& collection, const std::string& name) const;
        const CollectionDescriptor& require_collection(const std::string& name) const;
    };
}
