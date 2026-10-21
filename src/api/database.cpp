#include "vectordb/database.hpp"
#include "api/transaction.hpp"
#include "exec/executor.hpp"
#include "query/lexer/lexer.hpp"
#include "query/parser/parser.hpp"
#include "query/planner/planner.hpp"
#include "query/semantic/analyzer.hpp"
#include "storage/buffer_manager.hpp"
#include "storage/file_header.hpp"
#include "storage/pager.hpp"
#include "storage/storage_engine.hpp"
#include "wal/wal_writer.hpp"
#include "query/query_error.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace vectordb
{
    namespace
    {
        // No-steal means a transaction's dirty pages must all fit in the pool: 1024 pages = 4 MiB.
        constexpr size_t BUFFER_POOL_CAPACITY = 1024;
    }

    Database::Database(
        std::unique_ptr<Pager> pager,
        std::unique_ptr<FileHeader> header,
        std::unique_ptr<BufferManager> buffer_manager,
        std::unique_ptr<StorageEngine> storage_engine,
        std::unique_ptr<WalWriter> wal,
        Catalog catalog)
        : m_pager(std::move(pager))
        , m_header(std::move(header))
        , m_buffer_manager(std::move(buffer_manager))
        , m_storage_engine(std::move(storage_engine))
        , m_wal(std::move(wal))
        , m_catalog(std::move(catalog))
    {
    }

    Database::~Database() = default;
    Database::Database(Database&&) noexcept = default;
    Database& Database::operator=(Database&&) noexcept = default;

    Database Database::open(const std::string& path)
    {
        bool exists = std::filesystem::exists(path);

        if (!exists)
        {
            std::ofstream create_file(path, std::ios::binary);
            if (!create_file)
            {
                throw std::runtime_error("cannot create database file: " + path);
            }
        }

        auto pager = std::make_unique<Pager>(path);
        auto header = std::make_unique<FileHeader>();
        Catalog catalog;

        if (!exists)
        {
            *header = FileHeader::create();
            header->set_page_count(2); // page 0 = header, page 1 = catalog root
            header->set_catalog_root(1);

            auto header_buffer = header->serialise();
            pager->write_page(0, header_buffer);

            auto catalog_buffer = catalog.serialise();
            pager->write_page(header->catalog_root(), catalog_buffer);
        }
        else
        {
            auto header_buffer = pager->read_page(0);
            *header = FileHeader::deserialise(header_buffer);

            if (!header->valid())
            {
                throw std::runtime_error("corrupt or incompatible database file: " + path);
            }

            auto catalog_buffer = pager->read_page(header->catalog_root());
            catalog = Catalog::deserialise(catalog_buffer);
        }

        auto buffer_manager = std::make_unique<BufferManager>(*pager, BUFFER_POOL_CAPACITY);
        buffer_manager->set_no_steal(true);
        auto storage_engine = std::make_unique<StorageEngine>(*buffer_manager, *pager, *header);
        auto wal = std::make_unique<WalWriter>(path + ".wal");

        return Database(
            std::move(pager), std::move(header), std::move(buffer_manager),
            std::move(storage_engine), std::move(wal), std::move(catalog));
    }

    void Database::require_open() const
    {
        if (!m_pager) throw std::runtime_error("database is closed");
    }

    void Database::close()
    {
        if (!m_pager) return; // already closed

        if (m_txn)
        {
            m_txn->rollback_to(0, *m_storage_engine);
            m_txn.reset();
        }

        m_buffer_manager->flush_all();
        m_pager->sync();
        m_wal->reset(); // checkpoint: every committed change is now in the main file

        m_storage_engine.reset();
        m_buffer_manager.reset();
        m_wal.reset();
        m_pager.reset();
    }

    // ---------- SQL execution ----------

    ResultSet Database::execute(const std::string& sql)
    {
        require_open();

        Lexer lexer(sql);
        Parser parser(lexer.tokenize());
        Statement statement = parser.parse_statement();

        if (std::holds_alternative<BeginStmt>(statement))    { begin_transaction(); return ResultSet{}; }
        if (std::holds_alternative<CommitStmt>(statement))   { commit_active();     return ResultSet{}; }
        if (std::holds_alternative<RollbackStmt>(statement)) { rollback_active();   return ResultSet{}; }

        if (auto* create = std::get_if<CreateCollectionStmt>(&statement))
        {
            return run_create(*create);
        }

        return run_dml(statement);
    }

    void Database::begin_transaction()
    {
        if (m_txn)
        {
            throw std::runtime_error("transaction error: BEGIN inside a transaction (nesting is not supported)");
        }
        m_txn = std::make_unique<Transaction>(m_next_txn_id++);
    }

    void Database::commit_active()
    {
        if (!m_txn) throw std::runtime_error("transaction error: COMMIT with no active transaction");

        std::unique_ptr<Transaction> txn = std::move(m_txn);
        if (txn->ops().empty()) return; // read-only: nothing to make durable

        try
        {
            m_wal->commit(txn->id(), txn->ops());
        }
        catch (...)
        {
            txn->rollback_to(0, *m_storage_engine); // not durable, so undo the in-memory changes
            throw;
        }

        // The transaction is durable in the WAL now. A failure while flushing pages
        // is NOT a rollback; M7's recovery will redo it from the log.
        m_buffer_manager->flush_all();
    }

    void Database::rollback_active()
    {
        if (!m_txn) throw std::runtime_error("transaction error: ROLLBACK with no active transaction");

        m_txn->rollback_to(0, *m_storage_engine);
        m_txn.reset();
    }

    ResultSet Database::run_create(const CreateCollectionStmt& stmt)
    {
        if (m_txn)
        {
            throw std::runtime_error("CREATE COLLECTION is not allowed inside a transaction yet");
        }

        SemanticAnalyzer(m_catalog).analyze(Statement{stmt});

        std::vector<ColumnRef> schema;
        for (const ColumnDefNode& node : stmt.columns)
        {
            for (const ColumnRef& existing : schema)
            {
                if (existing.name == node.name)
                {
                    throw QueryError("duplicate column name: " + node.name, 0);
                }
            }

            ColumnRef def;
            def.name = node.name;
            def.nullable = true; // grammar has no NOT NULL yet
            if (node.vector_dim.has_value())
            {
                def.type = ColumnType::Vector;
                def.vector_dim = *node.vector_dim;
            }
            else
            {
                def.type = *parse_column_type(node.type_name); // validated by the analyzer
            }
            schema.push_back(std::move(def));
        }

        create_collection(stmt.collection_name, std::move(schema));
        return ResultSet{};
    }

    ResultSet Database::run_dml(const Statement& statement)
    {
        SemanticAnalyzer(m_catalog).analyze(statement);
        LogicalPlan plan = Planner(m_catalog).plan(statement);

        bool auto_txn = (m_txn == nullptr);
        if (auto_txn) m_txn = std::make_unique<Transaction>(m_next_txn_id++);
        size_t mark = m_txn->mark();

        ResultSet result;
        try
        {
            ExecContext ctx{*m_storage_engine, *m_txn, [this] { persist_catalog(); }};
            result = Executor(m_catalog, ctx).execute(plan);
        }
        catch (...)
        {
            // Statement-level atomicity: undo just this statement's effects.
            m_txn->rollback_to(mark, *m_storage_engine);
            if (auto_txn) m_txn.reset();
            throw;
        }

        if (auto_txn) commit_active();
        return result;
    }

    // ---------- catalog + low-level API ----------

    const CollectionDescriptor& Database::create_collection(
        const std::string& name, std::vector<ColumnRef> schema)
    {
        const CollectionDescriptor& descriptor = m_catalog.create_collection(name, std::move(schema));
        persist_catalog();
        return descriptor;
    }

    const CollectionDescriptor* Database::find_collection(const std::string& name) const
    {
        return m_catalog.find_collection(name);
    }

    CollectionDescriptor& Database::require_collection(const std::string& name)
    {
        CollectionDescriptor* descriptor = m_catalog.find_collection(name);
        if (descriptor == nullptr) throw std::runtime_error("no such collection: " + name);
        return *descriptor;
    }

    RecordId Database::insert_record(const std::string& collection_name, const Record& record)
    {
        CollectionDescriptor& collection = require_collection(collection_name);
        uint32_t root_before = collection.root_page;

        RecordId id = m_storage_engine->insert_record(collection, record.serialise());
        if (collection.root_page != root_before) persist_catalog();

        m_buffer_manager->flush_all(); // engine no longer flushes; this API has no transaction
        return id;
    }

    std::optional<Record> Database::read_record(const std::string& collection_name, RecordId id)
    {
        require_collection(collection_name);
        auto bytes = m_storage_engine->read_record(id);
        if (!bytes.has_value()) return std::nullopt;
        return Record::deserialise(*bytes);
    }

    bool Database::delete_record(const std::string& collection_name, RecordId id)
    {
        require_collection(collection_name);
        bool deleted = m_storage_engine->delete_record(id);
        m_buffer_manager->flush_all();
        return deleted;
    }

    std::vector<std::pair<RecordId, Record>> Database::scan_collection(const std::string& collection_name)
    {
        const CollectionDescriptor& collection = require_collection(collection_name);
        auto raw = m_storage_engine->scan_collection(collection);

        std::vector<std::pair<RecordId, Record>> results;
        results.reserve(raw.size());
        for (auto& [id, bytes] : raw) results.emplace_back(id, Record::deserialise(bytes));
        return results;
    }

    void Database::persist_catalog()
    {
        auto buffer = m_catalog.serialise();
        m_pager->write_page(m_header->catalog_root(), buffer);
    }
}
