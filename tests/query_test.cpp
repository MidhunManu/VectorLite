#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <variant>

#include "catalog/catalog.hpp"
#include "catalog/schema.hpp"
#include "query/lexer/lexer.hpp"
#include "query/parser/parser.hpp"
#include "query/semantic/analyzer.hpp"
#include "query/query_error.hpp"

using namespace vectordb;

static Statement parse(const std::string& sql)
{
    return Parser(Lexer(sql).tokenize()).parse_statement();
}

// Returns the error offset if fn throws QueryError, or -1 if it doesn't throw.
static long throws_at(const std::function<void()>& fn)
{
    try { fn(); } catch (const QueryError& e) { return static_cast<long>(e.offset()); }
    return -1;
}

static Catalog make_catalog()
{
    Catalog c;
    c.create_collection("docs", {
        ColumnRef{"id",    ColumnType::Int64,  0, false},
        ColumnRef{"title", ColumnType::String, 0, false},
        ColumnRef{"emb",   ColumnType::Vector, 3, false},
    });
    return c;
}

TEST(Lexer, Basic)
{
    auto t = Lexer("select * FROM docs WHERE id >= 1").tokenize();
    EXPECT_EQ(t.size(), 9u);                     // 8 tokens + EOF         (fixed)
    EXPECT_EQ(t[0].type, TokenType::Select);     // keywords are case-insensitive
    EXPECT_EQ(t[6].type, TokenType::Ge);         // was t[5]               (fixed)
    EXPECT_EQ(t.back().type, TokenType::Eof);

    EXPECT_EQ(throws_at([] { Lexer("SELECT 'oops").tokenize(); }), 7);  // unterminated string
    EXPECT_EQ(throws_at([] { Lexer("SELECT @").tokenize(); }), 7);      // bad character
}

TEST(Parser, Statements)
{
    auto create = std::get<CreateCollectionStmt>(
        parse("CREATE COLLECTION docs (id INT64, emb FLOAT32 VECTOR(3))"));
    EXPECT_EQ(create.columns.size(), 2u);
    EXPECT_EQ(create.columns[1].vector_dim, 3u);

    auto ins = std::get<InsertStmt>(
        parse("INSERT INTO docs (id, title, emb) VALUES (1, 'it''s', ?)"));
    EXPECT_EQ(ins.values.size(), 3u);
    EXPECT_EQ(std::get<std::string>(std::get<LiteralValue>(ins.values[1])), "it's");
    EXPECT_TRUE(std::holds_alternative<BindParam>(ins.values[2]));

    auto sel = std::get<SelectStmt>(parse(
        "SELECT id FROM docs WHERE id > 1 AND title = 'x' "
        "ORDER BY VECTOR_DISTANCE(emb, ?) LIMIT 5"));
    EXPECT_EQ(sel.where->comparisons.size(), 2u);
    EXPECT_EQ(sel.where->connectors[0], LogicalOp::And);
    EXPECT_EQ(sel.order_by->column, "emb");
    EXPECT_EQ(sel.limit, 5);

    EXPECT_TRUE(std::holds_alternative<BeginStmt>(parse("BEGIN")));
}

TEST(Parser, Errors)
{
    EXPECT_EQ(throws_at([] { parse("SELECT FROM docs"); }), 7);   // points at FROM
    EXPECT_GE(throws_at([] { parse("INSERT INTO docs (a, b) VALUES (1)"); }), 0);
    EXPECT_EQ(throws_at([] { parse("FROBNICATE"); }), 0);
}

TEST(Analyzer, Valid)
{
    Catalog cat = make_catalog();
    SemanticAnalyzer an(cat);
    auto check = [&](const std::string& sql) { an.analyze(parse(sql)); };

    EXPECT_NO_THROW(check("SELECT id FROM docs WHERE id = 1"));
    EXPECT_NO_THROW(check("INSERT INTO docs (id, title, emb) VALUES (1, 'a', ?)"));
    EXPECT_NO_THROW(check("SELECT * FROM docs ORDER BY VECTOR_DISTANCE(emb, ?) LIMIT 3"));
}

TEST(Analyzer, Errors)
{
    Catalog cat = make_catalog();
    SemanticAnalyzer an(cat);
    auto check = [&](const std::string& sql) { an.analyze(parse(sql)); };

    EXPECT_GE(throws_at([&] { check("SELECT * FROM nope"); }), 0);                 // unknown collection
    EXPECT_GE(throws_at([&] { check("SELECT bad FROM docs"); }), 0);               // unknown column
    EXPECT_GE(throws_at([&] { check("SELECT * FROM docs WHERE id = 'x'"); }), 0);  // type mismatch
    EXPECT_GE(throws_at([&] { check("SELECT * FROM docs ORDER BY VECTOR_DISTANCE(title, ?)"); }), 0); // non-vector
    EXPECT_GE(throws_at([&] { check("INSERT INTO docs (id, title, emb) VALUES (1, 'a', 2)"); }), 0);  // vector must be '?' (fixed)
    EXPECT_GE(throws_at([&] { check("CREATE COLLECTION docs (id INT64)"); }), 0);  // duplicate collection
}
