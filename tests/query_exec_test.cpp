#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

#include "query/query_error.hpp"
#include "vectordb/database.hpp"

using namespace vectordb;
namespace fs = std::filesystem;
using Ids = std::vector<int64_t>;
using Names = std::vector<std::string>;

struct TempDb
{
    std::string path;
    explicit TempDb(const std::string& name)
        : path((fs::temp_directory_path() / ("vdb_m5_" + name + ".vdb")).string()) { cleanup(); }
    ~TempDb() { cleanup(); }
    void cleanup() { std::error_code ec; fs::remove(path, ec); fs::remove(path + ".wal", ec); }
};

// Sorted first-column ids (storage order changes after UPDATE, so sort for stable comparison).
static Ids ids(Database& db, const std::string& sql)
{
    Ids out;
    for (const auto& row : db.execute(sql).rows) out.push_back(std::get<int64_t>(row[0].data));
    std::sort(out.begin(), out.end());
    return out;
}

// Variadic so braces with commas survive the preprocessor: EXPECT_IDS(db, "SELECT ...", 1, 3)
#define EXPECT_IDS(db, sql, ...) EXPECT_EQ(ids(db, sql), (Ids{__VA_ARGS__}))
#define EXPECT_FAILS(expr) EXPECT_THROW(expr, std::exception)

static void seed(Database& db)
{
    db.execute("CREATE COLLECTION t (id INT64, title STRING, score FLOAT32)");
    db.execute("BEGIN");
    db.execute("INSERT INTO t (id, title, score) VALUES (1, 'alpha', 0.5)");
    db.execute("INSERT INTO t (id, title, score) VALUES (2, 'beta', 0.9)");
    db.execute("INSERT INTO t (id, title, score) VALUES (3, 'gamma', 0.1)");
    db.execute("COMMIT");
}

TEST(Execution, InsertSelectRoundtrip)
{
    TempDb f("roundtrip");
    Database db = Database::open(f.path);
    seed(db);

    ResultSet all = db.execute("SELECT * FROM t");
    EXPECT_EQ(all.columns, (Names{"id", "title", "score"}));
    EXPECT_EQ(all.rows.size(), 3u);

    db.execute("INSERT INTO t (id, title) VALUES (4, 'it''s')");   // escaped quote
    EXPECT_IDS(db, "SELECT id FROM t WHERE title = 'it''s'", 4);
    db.close();
}

TEST(Execution, ComparisonOps)
{
    TempDb f("cmp");
    Database db = Database::open(f.path);
    seed(db);

    EXPECT_IDS(db, "SELECT id FROM t WHERE id = 2", 2);
    EXPECT_IDS(db, "SELECT id FROM t WHERE id != 2", 1, 3);
    EXPECT_IDS(db, "SELECT id FROM t WHERE id < 2", 1);
    EXPECT_IDS(db, "SELECT id FROM t WHERE id > 2", 3);
    EXPECT_IDS(db, "SELECT id FROM t WHERE id <= 2", 1, 2);
    EXPECT_IDS(db, "SELECT id FROM t WHERE id >= 2", 2, 3);
    EXPECT_IDS(db, "SELECT id FROM t WHERE title > 'beta'", 3);
    EXPECT_IDS(db, "SELECT id FROM t WHERE score > 0.4", 1, 2);   // float column vs float literal
    db.close();
}

TEST(Execution, AndOrPrecedence)
{
    TempDb f("prec");
    Database db = Database::open(f.path);
    seed(db);

    EXPECT_IDS(db, "SELECT id FROM t WHERE id = 1 OR id = 2", 1, 2);
    EXPECT_IDS(db, "SELECT id FROM t WHERE id = 1 AND id = 2");
    // AND binds tighter than OR:
    EXPECT_IDS(db, "SELECT id FROM t WHERE id = 1 OR id = 2 AND id = 3", 1);   // 1 OR (2 AND 3)
    EXPECT_IDS(db, "SELECT id FROM t WHERE id = 3 AND id = 1 OR id = 2", 2);   // (3 AND 1) OR 2
    EXPECT_IDS(db, "SELECT id FROM t WHERE score > 0.4 AND id < 3 OR title = 'gamma'", 1, 2, 3);
    db.close();
}

TEST(Execution, ProjectionAndLimit)
{
    TempDb f("projlimit");
    Database db = Database::open(f.path);
    seed(db);

    ResultSet r = db.execute("SELECT title, id FROM t WHERE id = 1");
    EXPECT_EQ(r.columns, (Names{"title", "id"}));
    ASSERT_EQ(r.rows.size(), 1u);
    ASSERT_EQ(r.rows[0].size(), 2u);
    EXPECT_EQ(std::get<std::string>(r.rows[0][0].data), "alpha");
    EXPECT_EQ(std::get<int64_t>(r.rows[0][1].data), 1);

    EXPECT_EQ(db.execute("SELECT id FROM t LIMIT 2").rows.size(), 2u);
    EXPECT_EQ(db.execute("SELECT id FROM t LIMIT 0").rows.size(), 0u);
    EXPECT_EQ(db.execute("SELECT id FROM t LIMIT 99").rows.size(), 3u);
    db.close();
}

TEST(Execution, Update)
{
    TempDb f("update");
    Database db = Database::open(f.path);
    seed(db);

    EXPECT_EQ(db.execute("UPDATE t SET title = 'BETA', score = 1 WHERE id = 2").affected_rows, 1);
    ResultSet r = db.execute("SELECT title, score FROM t WHERE id = 2");
    ASSERT_EQ(r.rows.size(), 1u);                                          // old version is gone
    EXPECT_EQ(std::get<std::string>(r.rows[0][0].data), "BETA");
    EXPECT_FLOAT_EQ(std::get<float>(r.rows[0][1].data), 1.0f);             // int literal coerced to FLOAT32

    EXPECT_EQ(db.execute("UPDATE t SET title = 'x' WHERE id = 99").affected_rows, 0);
    EXPECT_EQ(db.execute("UPDATE t SET title = 'same'").affected_rows, 3); // no WHERE: all rows, no infinite loop
    EXPECT_IDS(db, "SELECT id FROM t WHERE title = 'same'", 1, 2, 3);
    db.close();
}

TEST(Execution, Delete)
{
    TempDb f("delete");
    Database db = Database::open(f.path);
    seed(db);

    EXPECT_EQ(db.execute("DELETE FROM t WHERE id = 99").affected_rows, 0);
    EXPECT_EQ(db.execute("DELETE FROM t WHERE id = 2").affected_rows, 1);
    EXPECT_IDS(db, "SELECT id FROM t", 1, 3);
    EXPECT_EQ(db.execute("DELETE FROM t").affected_rows, 2);
    EXPECT_IDS(db, "SELECT id FROM t");
    db.close();
}

TEST(Execution, NullSemantics)
{
    TempDb f("null");
    Database db = Database::open(f.path);
    seed(db);

    db.execute("INSERT INTO t (id, title) VALUES (4, 'no score')");        // score = NULL
    EXPECT_IDS(db, "SELECT id FROM t WHERE score < 100", 1, 2, 3);         // NULL never matches
    EXPECT_IDS(db, "SELECT id FROM t WHERE score != 0.5", 2, 3);           // not even !=
    EXPECT_IDS(db, "SELECT id FROM t", 1, 2, 3, 4);
    db.close();
}

TEST(Transactions, Rollback)
{
    TempDb f("rollback");
    Database db = Database::open(f.path);
    seed(db);

    db.execute("BEGIN");
    db.execute("INSERT INTO t (id, title) VALUES (4, 'new')");
    db.execute("DELETE FROM t WHERE id = 1");
    db.execute("UPDATE t SET title = 'CHANGED' WHERE id = 2");
    EXPECT_IDS(db, "SELECT id FROM t", 2, 3, 4);                           // txn sees its own writes
    db.execute("ROLLBACK");

    EXPECT_IDS(db, "SELECT id FROM t", 1, 2, 3);
    ResultSet r = db.execute("SELECT title FROM t WHERE id = 2");
    ASSERT_EQ(r.rows.size(), 1u);
    EXPECT_EQ(std::get<std::string>(r.rows[0][0].data), "beta");
    db.close();
}

TEST(Transactions, CommitPersistsAcrossReopen)
{
    TempDb f("persist");
    {
        Database db = Database::open(f.path);
        seed(db);
        db.execute("BEGIN");
        db.execute("INSERT INTO t (id, title) VALUES (4, 'four')");
        db.execute("DELETE FROM t WHERE id = 1");
        db.execute("UPDATE t SET title = 'BETA' WHERE id = 2");
        db.execute("COMMIT");
        db.close();
        EXPECT_EQ(fs::file_size(f.path + ".wal"), 0u);                     // checkpoint truncated the WAL
    }
    Database db = Database::open(f.path);
    EXPECT_IDS(db, "SELECT id FROM t", 2, 3, 4);
    ResultSet r = db.execute("SELECT title FROM t WHERE id = 2");
    ASSERT_EQ(r.rows.size(), 1u);
    EXPECT_EQ(std::get<std::string>(r.rows[0][0].data), "BETA");
    db.close();
}

TEST(Transactions, WalWrittenOnCommit)
{
    TempDb f("walsize");
    Database db = Database::open(f.path);
    seed(db);                                                              // committed, not yet checkpointed
    EXPECT_GT(fs::file_size(f.path + ".wal"), 0u);
    db.close();
    EXPECT_EQ(fs::file_size(f.path + ".wal"), 0u);
}

TEST(Transactions, UncommittedDiscardedOnClose)
{
    TempDb f("uncommitted");
    {
        Database db = Database::open(f.path);
        seed(db);
        db.execute("BEGIN");
        db.execute("INSERT INTO t (id, title) VALUES (9, 'ghost')");
        db.execute("DELETE FROM t WHERE id = 1");
        db.close();                                                        // open txn must be rolled back
    }
    Database db = Database::open(f.path);
    EXPECT_IDS(db, "SELECT id FROM t", 1, 2, 3);
    db.close();
}

TEST(Transactions, Errors)
{
    TempDb f("txnerr");
    Database db = Database::open(f.path);
    seed(db);

    EXPECT_FAILS(db.execute("COMMIT"));                                    // no active txn
    EXPECT_FAILS(db.execute("ROLLBACK"));
    db.execute("BEGIN");
    EXPECT_FAILS(db.execute("BEGIN"));                                     // no nesting
    EXPECT_FAILS(db.execute("CREATE COLLECTION u (a INT64)"));             // DDL not allowed in txn (yet)
    db.execute("ROLLBACK");
    EXPECT_IDS(db, "SELECT id FROM t", 1, 2, 3);                           // still healthy afterwards
    db.close();
}

TEST(Transactions, StatementAtomicityInsideTxn)
{
    TempDb f("stmtatomic");
    Database db = Database::open(f.path);
    db.execute("CREATE COLLECTION t (id INT64, title STRING, score FLOAT32)");

    db.execute("BEGIN");
    db.execute("INSERT INTO t (id, title) VALUES (1, 'ok')");
    EXPECT_FAILS(db.execute("INSERT INTO t (id, title) VALUES (2, '" + std::string(5000, 'x') + "')"));
    db.execute("INSERT INTO t (id, title) VALUES (3, 'also ok')");         // txn still usable after failure
    db.execute("COMMIT");

    EXPECT_IDS(db, "SELECT id FROM t", 1, 3);
    db.close();
}

TEST(Storage, OversizedRecordRejected)
{
    TempDb f("oversize");
    Database db = Database::open(f.path);
    db.execute("CREATE COLLECTION t (id INT64, title STRING, score FLOAT32)");

    // Regression for the M3 infinite page-allocation loop: must throw, not hang.
    EXPECT_FAILS(db.execute("INSERT INTO t (id, title) VALUES (1, '" + std::string(5000, 'x') + "')"));
    EXPECT_IDS(db, "SELECT id FROM t");

    db.execute("INSERT INTO t (id, title) VALUES (2, '" + std::string(3000, 'y') + "')");   // fits
    EXPECT_IDS(db, "SELECT id FROM t", 2);
    db.close();
}

TEST(Storage, ManyRowsSpanPages)
{
    TempDb f("manyrows");
    const int N = 300;                                                     // ~10 pages at 100-byte titles
    {
        Database db = Database::open(f.path);
        db.execute("CREATE COLLECTION t (id INT64, title STRING, score FLOAT32)");
        db.execute("BEGIN");                                               // one txn = 2 fsyncs total, not 600
        for (int i = 0; i < N; ++i)
        {
            db.execute("INSERT INTO t (id, title) VALUES (" + std::to_string(i) + ", '" + std::string(100, 'a') + "')");
        }
        db.execute("COMMIT");
        EXPECT_EQ(db.execute("SELECT id FROM t").rows.size(), static_cast<size_t>(N));
        EXPECT_EQ(db.execute("DELETE FROM t WHERE id >= 100").affected_rows, N - 100);
        db.close();
    }
    Database db = Database::open(f.path);
    EXPECT_EQ(db.execute("SELECT id FROM t").rows.size(), 100u);
    EXPECT_IDS(db, "SELECT id FROM t WHERE id = 42", 42);
    db.close();
}

TEST(Execution, ErrorCases)
{
    TempDb f("errors");
    Database db = Database::open(f.path);
    seed(db);

    EXPECT_FAILS(db.execute("SELECT * FROM nope"));                        // unknown collection
    EXPECT_FAILS(db.execute("SELECT nope FROM t"));                        // unknown column
    EXPECT_FAILS(db.execute("SELECT * FROM t WHERE nope = 1"));
    EXPECT_FAILS(db.execute("SELECT * FROM t WHERE id = 'x'"));            // type mismatch
    EXPECT_FAILS(db.execute("INSERT INTO t (id) VALUES ('x')"));
    EXPECT_FAILS(db.execute("INSERT INTO t (id, id) VALUES (1, 2)"));      // duplicate column
    EXPECT_FAILS(db.execute("INSERT INTO t (id, nope) VALUES (5, 1)"));
    EXPECT_FAILS(db.execute("SELECT * FROM t garbage"));                   // trailing tokens
    EXPECT_FAILS(db.execute("SELEKT * FROM t"));                           // syntax error
    EXPECT_FAILS(db.execute("CREATE COLLECTION t (a INT64)"));             // duplicate collection
    EXPECT_FAILS(db.execute("CREATE COLLECTION u (a INT64, a INT64)"));    // duplicate column name
    EXPECT_FAILS(db.execute("SELECT * FROM t ORDER BY VECTOR_DISTANCE(id, ?)"));   // M6 feature

    EXPECT_IDS(db, "SELECT id FROM t", 1, 2, 3);                           // nothing leaked from failed statements
    db.close();
    EXPECT_FAILS(db.execute("SELECT * FROM t"));                           // closed
}
