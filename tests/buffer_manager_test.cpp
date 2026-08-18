#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>

#include "storage/pager.hpp"
#include "storage/buffer_manager.hpp"
#include "storage/slotted_page.hpp"
#include "storage/page.hpp"

using namespace vectordb;

namespace
{
    class BufferManagerTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_path = (std::filesystem::temp_directory_path() / "vdb_buffer_manager_test.vdb").string();

            std::ofstream create_file(m_path, std::ios::binary);
            create_file.close();

            m_pager = std::make_unique<Pager>(m_path);
        }

        void TearDown() override
        {
            m_pager.reset();
            std::remove(m_path.c_str());
        }

        std::string m_path;
        std::unique_ptr<Pager> m_pager;
    };
}

TEST_F(BufferManagerTest, NewPageIsCachedAndReadable)
{
    BufferManager bm(*m_pager, 4);

    auto& page = bm.new_page(0);
    SlottedPage::init(page, 0, PageType::Data);

    EXPECT_EQ(SlottedPage::page_id(page), 0u);
    bm.unpin_page(0, true);
}

TEST_F(BufferManagerTest, FlushWritesDirtyPageToDisk)
{
    BufferManager bm(*m_pager, 4);

    auto& page = bm.new_page(1);
    SlottedPage::init(page, 1, PageType::Data);
    SlottedPage::insert_record(page, std::vector<char>{'h', 'i'});

    bm.unpin_page(1, true);
    bm.flush_page(1);

    auto raw = m_pager->read_page(1);
    EXPECT_EQ(SlottedPage::page_id(raw), 1u);
    EXPECT_EQ(SlottedPage::slot_count(raw), 1u);
}

TEST_F(BufferManagerTest, FetchPageReturnsSameCachedFrame)
{
    BufferManager bm(*m_pager, 4);

    auto& first = bm.new_page(2);
    SlottedPage::init(first, 2, PageType::Data);
    bm.unpin_page(2, true);
    bm.flush_page(2);

    auto& second = bm.fetch_page(2);
    EXPECT_EQ(&first, &second);
    bm.unpin_page(2);
}

TEST_F(BufferManagerTest, EvictsUnpinnedPageWhenCapacityExceeded)
{
    BufferManager bm(*m_pager, 2);

    auto& p0 = bm.new_page(0);
    SlottedPage::init(p0, 0, PageType::Data);
    bm.unpin_page(0, true);
    bm.flush_page(0);

    auto& p1 = bm.new_page(1);
    SlottedPage::init(p1, 1, PageType::Data);
    bm.unpin_page(1, true);
    bm.flush_page(1);

    auto& p2 = bm.new_page(2);
    SlottedPage::init(p2, 2, PageType::Data);
    bm.unpin_page(2, true);
    bm.flush_page(2);

    auto& p0_again = bm.fetch_page(0);
    EXPECT_EQ(SlottedPage::page_id(p0_again), 0u);
    bm.unpin_page(0);
}

TEST_F(BufferManagerTest, PinnedPageIsNotEvicted)
{
    BufferManager bm(*m_pager, 1);

    auto& p0 = bm.new_page(0);
    SlottedPage::init(p0, 0, PageType::Data);
    EXPECT_THROW(bm.new_page(1), std::runtime_error);

    bm.unpin_page(0, true);
    bm.flush_page(0);
}
