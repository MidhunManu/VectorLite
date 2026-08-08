#include <filesystem>
#include <gtest/gtest.h>
#include "vectordb/database.hpp"
#include "../src/catalog/schema.hpp"
#include "../src/catalog/collection_descriptor.hpp"

namespace
{
	const std::filesystem::path test_database = "test.db";
}

TEST(DatabaseTest, CreateDatabaseTest)
{
	std::filesystem::remove(test_database);
	auto db = vectordb::Database::open(test_database.string());
	EXPECT_TRUE(std::filesystem::exists(test_database));
	db.close();
	std::filesystem::remove(test_database);
}

TEST(Database, OpenExistingDatabase)
{
	std::filesystem::remove(test_database);
	{
		auto db = vectordb::Database::open(test_database.string());
		db.close();
	}
	ASSERT_TRUE(std::filesystem::exists(test_database));
	{
		auto db = vectordb::Database::open(test_database.string());
		db.close();
	}
	std::filesystem::remove(test_database);
}

// --- Milestone 2: Collections & Catalog ---

TEST(CatalogTest, CreateCollectionReturnsDescriptor)
{
	std::filesystem::remove(test_database);
	auto db = vectordb::Database::open(test_database.string());

	std::vector<vectordb::ColumnRef> schema{
		vectordb::ColumnRef{.name = "id", .type = vectordb::ColumnType::Int32},
		vectordb::ColumnRef{.name = "embedding", .type = vectordb::ColumnType::Vector, .vector_dim = 128},
	};

	const auto& descriptor = db.create_collection("documents", schema);

	EXPECT_EQ(descriptor.name, "documents");
	EXPECT_EQ(descriptor.id, 1u);
	ASSERT_EQ(descriptor.schema.size(), 2u);
	EXPECT_EQ(descriptor.schema[0].name, "id");
	EXPECT_EQ(descriptor.schema[1].vector_dim, 128u);

	db.close();
	std::filesystem::remove(test_database);
}

TEST(CatalogTest, DuplicateCollectionNameThrows)
{
	std::filesystem::remove(test_database);
	auto db = vectordb::Database::open(test_database.string());

	std::vector<vectordb::ColumnRef> schema{
		vectordb::ColumnRef{.name = "id", .type = vectordb::ColumnType::Int32},
	};

	db.create_collection("documents", schema);

	EXPECT_THROW(db.create_collection("documents", schema), std::runtime_error);

	db.close();
	std::filesystem::remove(test_database);
}

TEST(CatalogTest, FindNonexistentCollectionReturnsNullptr)
{
	std::filesystem::remove(test_database);
	auto db = vectordb::Database::open(test_database.string());

	EXPECT_EQ(db.find_collection("does_not_exist"), nullptr);

	db.close();
	std::filesystem::remove(test_database);
}

TEST(CatalogTest, SchemaRoundTripsThroughReopen)
{
	std::filesystem::remove(test_database);

	{
		auto db = vectordb::Database::open(test_database.string());

		std::vector<vectordb::ColumnRef> schema{
			vectordb::ColumnRef{.name = "id", .type = vectordb::ColumnType::Int32, .nullable = false},
			vectordb::ColumnRef{.name = "title", .type = vectordb::ColumnType::String, .nullable = true},
			vectordb::ColumnRef{.name = "embedding", .type = vectordb::ColumnType::Vector, .vector_dim = 384},
		};

		db.create_collection("documents", schema);
		db.close();
	}

	ASSERT_TRUE(std::filesystem::exists(test_database));

	{
		auto db = vectordb::Database::open(test_database.string());

		const auto* descriptor = db.find_collection("documents");
		ASSERT_NE(descriptor, nullptr);

		EXPECT_EQ(descriptor->name, "documents");
		ASSERT_EQ(descriptor->schema.size(), 3u);

		EXPECT_EQ(descriptor->schema[0].name, "id");
		EXPECT_EQ(descriptor->schema[0].type, vectordb::ColumnType::Int32);
		EXPECT_FALSE(descriptor->schema[0].nullable);

		EXPECT_EQ(descriptor->schema[1].name, "title");
		EXPECT_EQ(descriptor->schema[1].type, vectordb::ColumnType::String);
		EXPECT_TRUE(descriptor->schema[1].nullable);

		EXPECT_EQ(descriptor->schema[2].name, "embedding");
		EXPECT_EQ(descriptor->schema[2].type, vectordb::ColumnType::Vector);
		EXPECT_EQ(descriptor->schema[2].vector_dim, 384u);

		db.close();
	}

	std::filesystem::remove(test_database);
}

TEST(CatalogTest, MultipleCollectionsPersistIndependently)
{
	std::filesystem::remove(test_database);

	{
		auto db = vectordb::Database::open(test_database.string());

		db.create_collection("documents", {
			vectordb::ColumnRef{.name = "embedding", .type = vectordb::ColumnType::Vector, .vector_dim = 64},
		});
		db.create_collection("users", {
			vectordb::ColumnRef{.name = "name", .type = vectordb::ColumnType::String},
		});

		db.close();
	}

	{
		auto db = vectordb::Database::open(test_database.string());

		const auto* documents = db.find_collection("documents");
		const auto* users = db.find_collection("users");

		ASSERT_NE(documents, nullptr);
		ASSERT_NE(users, nullptr);
		EXPECT_NE(documents->id, users->id);
		EXPECT_EQ(documents->schema[0].vector_dim, 64u);
		EXPECT_EQ(users->schema[0].name, "name");

		db.close();
	}

	std::filesystem::remove(test_database);
}