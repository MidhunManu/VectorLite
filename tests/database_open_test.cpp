#include <filesystem>
#include <gtest/gtest.h>
#include "vectordb/database.hpp"

namespace
{
	const std::filesystem::path test_database = "test.db";
}

TEST(DatabaseTest, OpenDataBaseSuccessfully)
{
	auto db = vectordb::Database::open("test.db");
	SUCCEED();
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
		auto db = vectordb::Database::open(test_database);
		db.close();
	}
	ASSERT_TRUE(std::filesystem::exists(test_database));
	{
		auto db = vectordb::Database::open(test_database.string());
		db.close();
	}
	std::filesystem::remove(test_database);
}
