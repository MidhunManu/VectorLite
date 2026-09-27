#include "storage/slotted_page.hpp"
#include "vectordb/config.hpp"

#include <cstring>

namespace vectordb
{
    namespace
    {
        void write_u16_at(std::vector<char>& buf, size_t offset, uint16_t value)
        {
            std::memcpy(buf.data() + offset, &value, sizeof(value));
        }

        void write_u32_at(std::vector<char>& buf, size_t offset, uint32_t value)
        {
            std::memcpy(buf.data() + offset, &value, sizeof(value));
        }

        void write_u8_at(std::vector<char>& buf, size_t offset, uint8_t value)
        {
            buf[offset] = static_cast<char>(value);
        }

        uint16_t read_u16_at(const std::vector<char>& buf, size_t offset)
        {
            uint16_t value;
            std::memcpy(&value, buf.data() + offset, sizeof(value));
            return value;
        }

        uint32_t read_u32_at(const std::vector<char>& buf, size_t offset)
        {
            uint32_t value;
            std::memcpy(&value, buf.data() + offset, sizeof(value));
            return value;
        }

        uint8_t read_u8_at(const std::vector<char>& buf, size_t offset)
        {
            return static_cast<uint8_t>(buf[offset]);
        }

        constexpr size_t OFF_PAGE_ID    = 0;
        constexpr size_t OFF_TYPE       = 4;
        constexpr size_t OFF_NEXT_PAGE  = 5;
        constexpr size_t OFF_SLOT_COUNT = 9;
        constexpr size_t OFF_FREE_START = 11;
        constexpr size_t OFF_FREE_END   = 13;
        constexpr size_t OFF_CHECKSUM   = 15;

        uint32_t fnv1a(const std::vector<char>& buffer)
        {
            uint32_t hash = 2166136261u;
            for (size_t i = 0; i < buffer.size(); ++i)
            {
                uint8_t byte;
                if (i >= OFF_CHECKSUM && i < OFF_CHECKSUM + sizeof(uint32_t))
                    byte = 0;
                else
                    byte = static_cast<uint8_t>(buffer[i]);
                hash ^= byte;
                hash *= 16777619u;
            }
            return hash;
        }
    }

    void SlottedPage::init(std::vector<char>& buffer, uint32_t page_id, PageType type)
    {
        buffer.assign(PAGE_SIZE, 0);
        write_u32_at(buffer, OFF_PAGE_ID, page_id);
        write_u8_at(buffer, OFF_TYPE, static_cast<uint8_t>(type));
        write_u32_at(buffer, OFF_NEXT_PAGE, 0);
        write_u16_at(buffer, OFF_SLOT_COUNT, 0);
        write_u16_at(buffer, OFF_FREE_START, static_cast<uint16_t>(HEADER_SIZE));
        write_u16_at(buffer, OFF_FREE_END, static_cast<uint16_t>(PAGE_SIZE));
        write_checksum(buffer);
    }

    uint32_t SlottedPage::page_id(const std::vector<char>& buffer)
    {
        return read_u32_at(buffer, OFF_PAGE_ID);
    }

    PageType SlottedPage::type(const std::vector<char>& buffer)
    {
        return static_cast<PageType>(read_u8_at(buffer, OFF_TYPE));
    }

    uint32_t SlottedPage::next_page(const std::vector<char>& buffer)
    {
        return read_u32_at(buffer, OFF_NEXT_PAGE);
    }

    void SlottedPage::set_next_page(std::vector<char>& buffer, uint32_t next_page_id)
    {
        write_u32_at(buffer, OFF_NEXT_PAGE, next_page_id);
        write_checksum(buffer);
    }

    uint16_t SlottedPage::slot_count(const std::vector<char>& buffer)
    {
        return read_u16_at(buffer, OFF_SLOT_COUNT);
    }

    uint16_t SlottedPage::read_free_start(const std::vector<char>& buffer)
    {
        return read_u16_at(buffer, OFF_FREE_START);
    }

    uint16_t SlottedPage::read_free_end(const std::vector<char>& buffer)
    {
        return read_u16_at(buffer, OFF_FREE_END);
    }

    void SlottedPage::write_free_start(std::vector<char>& buffer, uint16_t value)
    {
        write_u16_at(buffer, OFF_FREE_START, value);
    }

    void SlottedPage::write_free_end(std::vector<char>& buffer, uint16_t value)
    {
        write_u16_at(buffer, OFF_FREE_END, value);
    }

    void SlottedPage::write_slot_count(std::vector<char>& buffer, uint16_t value)
    {
        write_u16_at(buffer, OFF_SLOT_COUNT, value);
    }

    size_t SlottedPage::free_space(const std::vector<char>& buffer)
    {
        uint16_t free_start = read_free_start(buffer);
        uint16_t free_end = read_free_end(buffer);
        if (free_end < free_start) return 0;
        return static_cast<size_t>(free_end - free_start);
    }

    size_t SlottedPage::max_record_size()
    {
        return PAGE_SIZE - HEADER_SIZE - SLOT_SIZE;
    }

    std::optional<uint16_t> SlottedPage::insert_record(
        std::vector<char>& buffer, const std::vector<char>& record)
    {
        size_t required = SLOT_SIZE + record.size();
        if (required > free_space(buffer))
        {
            return std::nullopt;
        }

        uint16_t free_end = read_free_end(buffer);
        uint16_t record_offset = static_cast<uint16_t>(free_end - record.size());

        std::memcpy(buffer.data() + record_offset, record.data(), record.size());

        uint16_t free_start = read_free_start(buffer);
        write_u16_at(buffer, free_start, record_offset);
        write_u16_at(buffer, static_cast<size_t>(free_start) + 2, static_cast<uint16_t>(record.size()));
        write_u8_at(buffer, static_cast<size_t>(free_start) + 4, 0);

        uint16_t slot_id = slot_count(buffer);
        write_slot_count(buffer, static_cast<uint16_t>(slot_id + 1));
        write_free_start(buffer, static_cast<uint16_t>(free_start + SLOT_SIZE));
        write_free_end(buffer, record_offset);

        write_checksum(buffer);
        return slot_id;
    }

    std::optional<std::vector<char>> SlottedPage::read_record(
        const std::vector<char>& buffer, uint16_t slot_id)
    {
        if (slot_id >= slot_count(buffer))
        {
            return std::nullopt;
        }

        size_t slot_offset = HEADER_SIZE + static_cast<size_t>(slot_id) * SLOT_SIZE;
        uint16_t record_offset = read_u16_at(buffer, slot_offset);
        uint16_t length = read_u16_at(buffer, slot_offset + 2);
        uint8_t flags = read_u8_at(buffer, slot_offset + 4);

        if (flags & TOMBSTONE_FLAG)
        {
            return std::nullopt;
        }

        std::vector<char> record(length);
        std::memcpy(record.data(), buffer.data() + record_offset, length);
        return record;
    }

    bool SlottedPage::delete_record(std::vector<char>& buffer, uint16_t slot_id)
    {
        if (slot_id >= slot_count(buffer))
        {
            return false;
        }

        size_t slot_offset = HEADER_SIZE + static_cast<size_t>(slot_id) * SLOT_SIZE;
        uint8_t flags = read_u8_at(buffer, slot_offset + 4);

        if (flags & TOMBSTONE_FLAG)
        {
            return false; // already deleted
        }

        write_u8_at(buffer, slot_offset + 4, flags | TOMBSTONE_FLAG);
        write_checksum(buffer);
        return true;
    }

    void SlottedPage::write_checksum(std::vector<char>& buffer)
    {
        write_u32_at(buffer, OFF_CHECKSUM, fnv1a(buffer));
    }

    bool SlottedPage::verify_checksum(const std::vector<char>& buffer)
    {
        return read_u32_at(buffer, OFF_CHECKSUM) == fnv1a(buffer);
    }
}