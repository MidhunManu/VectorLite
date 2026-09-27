#include "storage/storage_engine.hpp"
#include "storage/buffer_manager.hpp"
#include "storage/pager.hpp"
#include "storage/file_header.hpp"
#include "catalog/collection_descriptor.hpp"

#include <stdexcept>

namespace vectordb
{
    StorageEngine::StorageEngine(BufferManager& buffer_manager, Pager& pager, FileHeader& header)
        : m_buffer_manager(buffer_manager), m_pager(pager), m_header(header)
    {
    }

    uint32_t StorageEngine::allocate_page(PageType type)
    {
        uint32_t new_id = static_cast<uint32_t>(m_header.page_count());
        m_header.set_page_count(new_id + 1);

        auto header_buffer = m_header.serialise();
        m_pager.write_page(0, header_buffer);

        std::vector<char>& buffer = m_buffer_manager.new_page(new_id);
        SlottedPage::init(buffer, new_id, type);
        m_buffer_manager.unpin_page(new_id, true);
        m_buffer_manager.flush_page(new_id);

        return new_id;
    }

    RecordId StorageEngine::insert_record(
        CollectionDescriptor& collection, const std::vector<char>& record_bytes)
    {
        if (collection.root_page == 0)
        {
            collection.root_page = allocate_page(PageType::Data);
        }

        uint32_t page_id = collection.root_page;

        while (true)
        {
            std::vector<char>& buffer = m_buffer_manager.fetch_page(page_id);

            if (!SlottedPage::verify_checksum(buffer))
            {
                m_buffer_manager.unpin_page(page_id);
                throw std::runtime_error("corrupt page (checksum mismatch): page " + std::to_string(page_id));
            }

            auto slot = SlottedPage::insert_record(buffer, record_bytes);
            if (slot.has_value())
            {
                m_buffer_manager.unpin_page(page_id, true);
                m_buffer_manager.flush_page(page_id);
                return RecordId{page_id, *slot};
            }

            uint32_t next = SlottedPage::next_page(buffer);
            if (next != 0)
            {
                m_buffer_manager.unpin_page(page_id);
                page_id = next;
                continue;
            }

            uint32_t new_page_id = allocate_page(PageType::Data);
            SlottedPage::set_next_page(buffer, new_page_id);
            m_buffer_manager.unpin_page(page_id, true);
            m_buffer_manager.flush_page(page_id);

            page_id = new_page_id;
        }
    }

    std::optional<std::vector<char>> StorageEngine::read_record(const RecordId& id)
    {
        std::vector<char>& buffer = m_buffer_manager.fetch_page(id.page_id);

        if (!SlottedPage::verify_checksum(buffer))
        {
            m_buffer_manager.unpin_page(id.page_id);
            throw std::runtime_error("corrupt page (checksum mismatch): page " + std::to_string(id.page_id));
        }

        auto record = SlottedPage::read_record(buffer, id.slot_id);
        m_buffer_manager.unpin_page(id.page_id);
        return record;
    }

    bool StorageEngine::delete_record(const RecordId& id)
    {
        std::vector<char>& buffer = m_buffer_manager.fetch_page(id.page_id);

        if (!SlottedPage::verify_checksum(buffer))
        {
            m_buffer_manager.unpin_page(id.page_id);
            throw std::runtime_error("corrupt page (checksum mismatch): page " + std::to_string(id.page_id));
        }

        bool deleted = SlottedPage::delete_record(buffer, id.slot_id);
        m_buffer_manager.unpin_page(id.page_id, deleted);
        if (deleted)
        {
            m_buffer_manager.flush_page(id.page_id);
        }
        return deleted;
    }

    std::vector<std::pair<RecordId, std::vector<char>>> StorageEngine::scan_collection(
        const CollectionDescriptor& collection)
    {
        std::vector<std::pair<RecordId, std::vector<char>>> results;

        uint32_t page_id = collection.root_page;
        while (page_id != 0)
        {
            std::vector<char>& buffer = m_buffer_manager.fetch_page(page_id);

            if (!SlottedPage::verify_checksum(buffer))
            {
                m_buffer_manager.unpin_page(page_id);
                throw std::runtime_error("corrupt page (checksum mismatch): page " + std::to_string(page_id));
            }

            uint16_t count = SlottedPage::slot_count(buffer);
            for (uint16_t slot = 0; slot < count; ++slot)
            {
                auto record = SlottedPage::read_record(buffer, slot);
                if (record.has_value())
                {
                    results.emplace_back(RecordId{page_id, slot}, std::move(*record));
                }
            }

            uint32_t next = SlottedPage::next_page(buffer);
            m_buffer_manager.unpin_page(page_id);
            page_id = next;
        }

        return results;
    }
}