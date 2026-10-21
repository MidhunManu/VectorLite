#pragma once
#include <vector>
#include "query/lexer/token.hpp"
#include "query/parser/ast.hpp"

namespace vectordb
{
    class Parser
    {
    public:
        explicit Parser(std::vector<Token> tokens);
        Statement parse_statement();

    private:
        std::vector<Token> m_tokens;
        size_t m_pos = 0;
        uint32_t m_next_param_index = 0;

        const Token& peek(size_t ahead = 0) const;
        const Token& advance();
        bool check(TokenType type) const;
        const Token& expect(TokenType type, const std::string& context);

        CreateCollectionStmt parse_create_collection();
        ColumnDefNode parse_column_def();
        InsertStmt parse_insert();
        SelectStmt parse_select();
        std::optional<Condition> parse_optional_where();
        Condition parse_condition();
        Comparison parse_comparison();
        ComparisonOp parse_comparison_op();
        DeleteStmt parse_delete();
        UpdateStmt parse_update();
        Assignment parse_assignment();
        ValueExpr parse_value_expr();
        LiteralValue parse_literal();
        std::string unescape_string(std::string_view quoted) const;
    };
}
