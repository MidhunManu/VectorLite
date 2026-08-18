#include <gtest/gtest.h>

#include "storage/slotted_page.hpp"
#include "vectordb/config.hpp"

using namespace vectordb;

namespace
{
    std::vector<char> make_record(const std::string& text)
    {
        return std::vector<char>(text.begin(), text.end());
    }
}

TEST(SlottedPageTest, InitSetsUpEmptyPage)
{
    std::vector<char> page;
    SlottedPage::init(page, 7, PageType::Data);

    EXPECT_EQ(SlottedPage::page_id(page), 7u);
    EXPECT_EQ(SlottedPage::type(page), PageType::Data);
    EXPECT_EQ(SlottedPage::next_page(page), 0u);
    EXPECT_EQ(SlottedPage::slot_count(page), 0u);
    EXPECT_TRUE(SlottedPage::verify_checksum(page));

    std::vector<char> exact(SlottedPage::max_record_size(), 'x');
    std::vector<char> probe = page;
    EXPECT_TRUE(SlottedPage::insert_record(probe, exact).has_value());

    std::vector<char> oversize(SlottedPage::max_record_size() + 1, 'x');
    std::vector<char> probe2 = page;
    EXPECT_FALSE(SlottedPage::insert_record(probe2, oversize).has_value());
}

TEST(SlottedPageTest, InsertAndReadRoundTrips)
{
    std::vector<char> page;
    SlottedPage::init(page, 1, PageType::Data);

    auto record = make_record("hello world");
    auto slot = SlottedPage::insert_record(page, record);

    ASSERT_TRUE(slot.has_value());
    EXPECT_EQ(*slot, 0u);
    EXPECT_EQ(SlottedPage::slot_count(page), 1u);

    auto read_back = SlottedPage::read_record(page, *slot);
    ASSERT_TRUE(read_back.has_value());
    EXPECT_EQ(*read_back, record);
}

TEST(SlottedPageTest, MultipleInsertsGetDistinctSlots)
{
    std::vector<char> page;
    SlottedPage::init(page, 1, PageType::Data);

    auto slot0 = SlottedPage::insert_record(page, make_record("a"));
    auto slot1 = SlottedPage::insert_record(page, make_record("bb"));
    auto slot2 = SlottedPage::insert_record(page, make_record("ccc"));

    ASSERT_TRUE(slot0.has_value());
    ASSERT_TRUE(slot1.has_value());
    ASSERT_TRUE(slot2.has_value());
    EXPECT_EQ(*slot0, 0u);
    EXPECT_EQ(*slot1, 1u);
    EXPECT_EQ(*slot2, 2u);

    EXPECT_EQ(*SlottedPage::read_record(page, *slot0), make_record("a"));
    EXPECT_EQ(*SlottedPage::read_record(page, *slot1), make_record("bb"));
    EXPECT_EQ(*SlottedPage::read_record(page, *slot2), make_record("ccc"));
}

TEST(SlottedPageTest, DeleteTombstonesRecord)
{
    std::vector<char> page;
    SlottedPage::init(page, 1, PageType::Data);

    auto slot = SlottedPage::insert_record(page, make_record("temp"));
    ASSERT_TRUE(slot.has_value());

    EXPECT_TRUE(SlottedPage::delete_record(page, *slot));
    EXPECT_FALSE(SlottedPage::read_record(page, *slot).has_value());

    EXPECT_FALSE(SlottedPage::delete_record(page, *slot));
}

TEST(SlottedPageTest, ReadOrDeleteOutOfRangeSlotFails)
{
    std::vector<char> page;
    SlottedPage::init(page, 1, PageType::Data);

    EXPECT_FALSE(SlottedPage::read_record(page, 0).has_value());
    EXPECT_FALSE(SlottedPage::delete_record(page, 0));
}

TEST(SlottedPageTest, InsertFailsWhenPageIsFull)
{
    std::vector<char> page;
    SlottedPage::init(page, 1, PageType::Data);

    std::vector<char> big_record(SlottedPage::max_record_size(), 'x');
    auto slot = SlottedPage::insert_record(page, big_record);
    ASSERT_TRUE(slot.has_value());

    auto second = SlottedPage::insert_record(page, make_record("x"));
    EXPECT_FALSE(second.has_value());
}

TEST(SlottedPageTest, NextPageLinkPersists)
{
    std::vector<char> page;
    SlottedPage::init(page, 1, PageType::Data);

    SlottedPage::set_next_page(page, 42);
    EXPECT_EQ(SlottedPage::next_page(page), 42u);
    EXPECT_TRUE(SlottedPage::verify_checksum(page));
}

TEST(SlottedPageTest, ChecksumDetectsCorruption)
{
    std::vector<char> page;
    SlottedPage::init(page, 1, PageType::Data);
    SlottedPage::insert_record(page, make_record("data"));

    ASSERT_TRUE(SlottedPage::verify_checksum(page));

    page[100] ^= 0xFF;
    EXPECT_FALSE(SlottedPage::verify_checksum(page));
}
