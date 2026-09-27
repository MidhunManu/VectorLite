#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "storage/page.hpp"

namespace vectordb
{
    struct RecordId
    {
        uint32_t page_id;
        uint16_t slot_id;
    };

    class SlottedPage
    {
    public:
        static void init(std::vector<char>& buffer, uint32_t page_id, PageType type);

        static uint32_t page_id(const std::vector<char>& buffer);
        static PageType type(const std::vector<char>& buffer);
        static uint32_t next_page(const std::vector<char>& buffer);
        static void set_next_page(std::vector<char>& buffer, uint32_t next_page_id);
        static uint16_t slot_count(const std::vector<char>& buffer);

        static std::optional<uint16_t> insert_record(
            std::vector<char>& buffer, const std::vector<char>& record);

        static std::optional<std::vector<char>> read_record(
            const std::vector<char>& buffer, uint16_t slot_id);

        static bool delete_record(std::vector<char>& buffer, uint16_t slot_id);

        static void write_checksum(std::vector<char>& buffer);
        static bool verify_checksum(const std::vector<char>& buffer);

        static size_t free_space(const std::vector<char>& buffer);

        static size_t max_record_size();

    private:
        static constexpr size_t HEADER_SIZE = 19;
        static constexpr size_t SLOT_SIZE = 5;
        static constexpr uint8_t TOMBSTONE_FLAG = 0x1;

        static uint16_t read_free_start(const std::vector<char>& buffer);
        static uint16_t read_free_end(const std::vector<char>& buffer);
        static void write_free_start(std::vector<char>& buffer, uint16_t value);
        static void write_free_end(std::vector<char>& buffer, uint16_t value);
        static void write_slot_count(std::vector<char>& buffer, uint16_t value);
    };
}