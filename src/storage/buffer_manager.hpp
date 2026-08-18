#pragma once

#include <stdint.h>
#include <unordered_map>
#include <vector>

namespace vectordb
{
    class Pager;

    class BufferManager
    {
    public:
        BufferManager(Pager& pager, size_t capacity);
        std::vector<char>& fetch_page(uint32_t pagePid);
        std::vector<char>& new_page(uint32_t page_id);
        void unpin_page(uint32_t page_id, bool mark_dirty = false);
        void mark_dirty(uint32_t page_id);
        void flush_page(uint32_t page_id);
        void flush_all();

    private:
        struct Frame
        {
            std::vector<char> data;
            int pin_count = 0;
            bool dirty = false;
            uint64_t last_used = 0;
        };

        Frame& get_or_load(uint32_t page_id, bool from_disk);
        void evict_one();

        Pager& m_pager;
        size_t m_capacity;
        uint64_t m_clock;
        std::unordered_map<uint32_t, Frame> m_frames;
    };
}
