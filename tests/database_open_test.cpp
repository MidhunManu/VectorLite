#include <gtest/gtest.h>
#include "vectordb/database.hpp"

TEST(DatabaseTest, OpenDataBaseSuccessfully)
{
	auto db = vectordb::Database::open("test.db");
	SUCCEED();
}
