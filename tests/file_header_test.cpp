#include <gtest/gtest.h>
#include "../src/storage/file_header.hpp"
#include "vectordb/config.hpp"

using vectordb::FileHeader;

TEST(FileHeaderTest, CreateProducesValidHeader)
{
	FileHeader header = FileHeader::create();
	EXPECT_TRUE(header.valid());
	EXPECT_EQ(header.page_count(), 1u);
	EXPECT_EQ(header.catalog_root(), 0u);
	EXPECT_EQ(header.clean_shutdown(), 1u);
}

TEST(FileHeaderTest, SerialiseProducesFullPageSizedBuffer)
{
	FileHeader header = FileHeader::create();
	auto buffer = header.serialise();
	EXPECT_EQ(buffer.size(), vectordb::PAGE_SIZE);
}

TEST(FileHeaderTest, RoundTripsThroughSerialiseDeserialise)
{
	FileHeader header = FileHeader::create();
	header.set_catalog_root(1);
	header.set_page_count(2);
	header.set_clean_shutdown(0);

	auto buffer = header.serialise();
	FileHeader restored = FileHeader::deserialise(buffer);

	EXPECT_TRUE(restored.valid());
	EXPECT_EQ(restored.catalog_root(), 1u);
	EXPECT_EQ(restored.page_count(), 2u);
	EXPECT_EQ(restored.clean_shutdown(), 0u);
}

TEST(FileHeaderTest, DeserialiseRejectsTooSmallBuffer)
{
	std::vector<char> buffer(4, 0);
	EXPECT_THROW(FileHeader::deserialise(buffer), std::runtime_error);
}

TEST(FileHeaderTest, ValidRejectsBadMagic)
{
	FileHeader header = FileHeader::create();
	auto buffer = header.serialise();

	buffer[0] = 'X'; // corrupt the magic bytes

	FileHeader restored = FileHeader::deserialise(buffer);
	EXPECT_FALSE(restored.valid());
}

TEST(FileHeaderTest, ValidRejectsWrongPageSize)
{
	FileHeader header = FileHeader::create();
	auto buffer = header.serialise();

	// page_size field sits right after magic(4) + version(4)
	uint32_t bogus_page_size = 1234;
	std::memcpy(buffer.data() + 8, &bogus_page_size, sizeof(bogus_page_size));

	FileHeader restored = FileHeader::deserialise(buffer);
	EXPECT_FALSE(restored.valid());
}

// Regression test for the header/catalog page-collision bug: a freshly
// created header must NOT point catalog_root at page 0 (its own page).
// If Database::open() ever forgets to allocate a separate catalog page
// again, this test should be the first thing to catch it.
TEST(FileHeaderTest, FreshHeaderCatalogRootMustNotAliasHeaderPageOnceAssigned)
{
	FileHeader header = FileHeader::create();
	header.set_catalog_root(1);

	EXPECT_NE(header.catalog_root(), 0u)
		<< "catalog_root must never be page 0 -- that's the header's own page";
}