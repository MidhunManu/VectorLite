#include "query/parser/parser.hpp"
#include "query/query_error.hpp"
#include <cstdlib>

namespace vectordb
{
    Parser::Parser(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {}

    const Token& Parser::peek(size_t ahead) const
    {
        size_t idx = m_pos + ahead;
        return idx < m_tokens.size() ? m_tokens[idx] : m_tokens.back(); // back() is Eof
    }

    const Token& Parser::advance()
    {
        const Token& t = peek();
        if (m_pos < m_tokens.size() - 1) m_pos++;
        return t;
    }

    bool Parser::check(TokenType type) const { return peek().type == type; }

    const Token& Parser::expect(TokenType type, const std::string& context)
    {
        if (!check(type))
        {
            throw QueryError(
                "expected " + std::string(token_type_name(type)) + " " + context +
                ", got " + token_type_name(peek().type), peek().offset);
        }
        return advance();
    }

    // Public entry: parse one statement, then require end of input.
    Statement Parser::parse_statement()
    {
        Statement stmt = parse_statement_inner();
        if (!check(TokenType::Eof))
        {
            throw QueryError(std::string("unexpected ") + token_type_name(peek().type) +
                             " after end of statement", peek().offset);
        }
        return stmt;
    }

    Statement Parser::parse_statement_inner()
    {
        switch (peek().type)
        {
            case TokenType::Create: return parse_create_collection();
            case TokenType::Insert: return parse_insert();
            case TokenType::Select: return parse_select();
            case TokenType::Delete: return parse_delete();
            case TokenType::Update: return parse_update();
            case TokenType::Begin:  advance(); return BeginStmt{};
            case TokenType::Commit: advance(); return CommitStmt{};
            case TokenType::Rollback: advance(); return RollbackStmt{};
            default:
                throw QueryError(
                    std::string("expected a statement, got ") + token_type_name(peek().type),
                    peek().offset);
        }
    }

    CreateCollectionStmt Parser::parse_create_collection()
    {
        expect(TokenType::Create, "to begin CREATE COLLECTION");
        expect(TokenType::Collection, "after CREATE");

        CreateCollectionStmt stmt;
        stmt.collection_name = std::string(expect(TokenType::Identifier, "collection name").lexeme);

        expect(TokenType::LParen, "after collection name");
        stmt.columns.push_back(parse_column_def());
        while (check(TokenType::Comma))
        {
            advance();
            stmt.columns.push_back(parse_column_def());
        }
        expect(TokenType::RParen, "to close column list");
        return stmt;
    }

    ColumnDefNode Parser::parse_column_def()
    {
        ColumnDefNode col;
        col.name = std::string(expect(TokenType::Identifier, "column name").lexeme);
        col.type_name = std::string(expect(TokenType::Identifier, "column type").lexeme);

        if (check(TokenType::Vector))
        {
            advance();
            expect(TokenType::LParen, "after VECTOR");
            const Token& dim = expect(TokenType::IntegerLiteral, "vector dimension");
            col.vector_dim = static_cast<uint32_t>(std::strtoul(std::string(dim.lexeme).c_str(), nullptr, 10));
            expect(TokenType::RParen, "to close VECTOR(...)");
        }
        return col;
    }

    ValueExpr Parser::parse_value_expr()
    {
        if (check(TokenType::Param))
        {
            advance();
            return BindParam{m_next_param_index++};
        }
        return parse_literal();
    }

    LiteralValue Parser::parse_literal()
    {
        const Token& t = peek();
        switch (t.type)
        {
            case TokenType::IntegerLiteral:
                advance();
                return static_cast<int64_t>(std::strtoll(std::string(t.lexeme).c_str(), nullptr, 10));
            case TokenType::FloatLiteral:
                advance();
                return std::strtod(std::string(t.lexeme).c_str(), nullptr);
            case TokenType::StringLiteral:
                advance();
                return unescape_string(t.lexeme);
            default:
                throw QueryError(
                    std::string("expected a literal value, got ") + token_type_name(t.type), t.offset);
        }
    }

    std::string Parser::unescape_string(std::string_view quoted) const
    {
        std::string result;
        result.reserve(quoted.size());
        for (size_t i = 1; i + 1 < quoted.size(); ++i)
        {
            if (quoted[i] == '\'' && i + 1 < quoted.size() - 1 && quoted[i + 1] == '\'')
            {
                result.push_back('\'');
                ++i;
            }
            else
            {
                result.push_back(quoted[i]);
            }
        }
        return result;
    }

    InsertStmt Parser::parse_insert()
    {
        expect(TokenType::Insert, "to begin INSERT");
        expect(TokenType::Into, "after INSERT");

        InsertStmt stmt;
        stmt.collection_name = std::string(expect(TokenType::Identifier, "collection name").lexeme);

        expect(TokenType::LParen, "before column list");
        stmt.columns.push_back(std::string(expect(TokenType::Identifier, "column name").lexeme));
        while (check(TokenType::Comma))
        {
            advance();
            stmt.columns.push_back(std::string(expect(TokenType::Identifier, "column name").lexeme));
        }
        expect(TokenType::RParen, "to close column list");

        expect(TokenType::Values, "after column list");
        expect(TokenType::LParen, "before value list");
        stmt.values.push_back(parse_value_expr());
        while (check(TokenType::Comma))
        {
            advance();
            stmt.values.push_back(parse_value_expr());
        }
        expect(TokenType::RParen, "to close value list");

        if (stmt.columns.size() != stmt.values.size())
        {
            throw QueryError("column count does not match value count", peek().offset);
        }
        return stmt;
    }

    SelectStmt Parser::parse_select()
    {
        expect(TokenType::Select, "to begin SELECT");

        SelectStmt stmt;
        if (check(TokenType::Star))
        {
            advance();
            stmt.select_all = true;
        }
        else
        {
            stmt.select_all = false;
            stmt.columns.push_back(std::string(expect(TokenType::Identifier, "column name").lexeme));
            while (check(TokenType::Comma))
            {
                advance();
                stmt.columns.push_back(std::string(expect(TokenType::Identifier, "column name").lexeme));
            }
        }

        expect(TokenType::From, "after select list");
        stmt.from = std::string(expect(TokenType::Identifier, "collection name").lexeme);
        stmt.where = parse_optional_where();

        if (check(TokenType::Order))
        {
            advance();
            expect(TokenType::By, "after ORDER");
            expect(TokenType::VectorDistance, "in ORDER BY clause");
            expect(TokenType::LParen, "after VECTOR_DISTANCE");

            VectorDistanceExpr order;
            order.column = std::string(expect(TokenType::Identifier, "vector column name").lexeme);
            expect(TokenType::Comma, "between VECTOR_DISTANCE arguments");
            expect(TokenType::Param, "as VECTOR_DISTANCE's query vector (must be a bound '?')");
            order.param = BindParam{m_next_param_index++};
            expect(TokenType::RParen, "to close VECTOR_DISTANCE(...)");
            stmt.order_by = order;
        }

        if (check(TokenType::Limit))
        {
            advance();
            const Token& n = expect(TokenType::IntegerLiteral, "after LIMIT");
            stmt.limit = std::strtoll(std::string(n.lexeme).c_str(), nullptr, 10);
        }

        return stmt;
    }

    std::optional<Condition> Parser::parse_optional_where()
    {
        if (!check(TokenType::Where)) return std::nullopt;
        advance();
        return parse_condition();
    }

    Condition Parser::parse_condition()
    {
        Condition cond;
        cond.comparisons.push_back(parse_comparison());

        while (check(TokenType::And) || check(TokenType::Or))
        {
            cond.connectors.push_back(check(TokenType::And) ? LogicalOp::And : LogicalOp::Or);
            advance();
            cond.comparisons.push_back(parse_comparison());
        }
        return cond;
    }

    Comparison Parser::parse_comparison()
    {
        Comparison cmp;
        cmp.column.name = std::string(expect(TokenType::Identifier, "column name in condition").lexeme);
        cmp.op = parse_comparison_op();
        cmp.value = parse_literal();
        return cmp;
    }

    ComparisonOp Parser::parse_comparison_op()
    {
        switch (peek().type)
        {
            case TokenType::Eq: advance(); return ComparisonOp::Eq;
            case TokenType::Ne: advance(); return ComparisonOp::Ne;
            case TokenType::Lt: advance(); return ComparisonOp::Lt;
            case TokenType::Gt: advance(); return ComparisonOp::Gt;
            case TokenType::Le: advance(); return ComparisonOp::Le;
            case TokenType::Ge: advance(); return ComparisonOp::Ge;
            default:
                throw QueryError(
                    std::string("expected a comparison operator, got ") + token_type_name(peek().type),
                    peek().offset);
        }
    }

    DeleteStmt Parser::parse_delete()
    {
        expect(TokenType::Delete, "to begin DELETE");
        expect(TokenType::From, "after DELETE");

        DeleteStmt stmt;
        stmt.from = std::string(expect(TokenType::Identifier, "collection name").lexeme);
        stmt.where = parse_optional_where();
        return stmt;
    }

    UpdateStmt Parser::parse_update()
    {
        expect(TokenType::Update, "to begin UPDATE");

        UpdateStmt stmt;
        stmt.table = std::string(expect(TokenType::Identifier, "collection name").lexeme);

        expect(TokenType::Set, "after collection name");
        stmt.assignments.push_back(parse_assignment());
        while (check(TokenType::Comma))
        {
            advance();
            stmt.assignments.push_back(parse_assignment());
        }

        stmt.where = parse_optional_where();
        return stmt;
    }

    Assignment Parser::parse_assignment()
    {
        Assignment a;
        a.column = std::string(expect(TokenType::Identifier, "column name in SET clause").lexeme);
        expect(TokenType::Eq, "in SET clause assignment");
        a.value = parse_value_expr();
        return a;
    }
}