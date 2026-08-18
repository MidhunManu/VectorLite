#include "storage/buffer_manager.hpp"
#include "storage/pager.hpp"

#include <limits>
#include <stdexcept>

namespace vectordb
{
    BufferManager::BufferManager(Pager& pager, size_t capacity)
    : m_pager(pager),
      m_capacity(capacity)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument("buffer manager, capacity must be > 0");
        }
    }

    BufferManager::Frame& BufferManager::get_or_load(uint32_t page_id, bool from_disk)
    {
        auto it = m_frames.find(page_id);
        if (it != m_frames.end())
        {
            it->second.pin_count++;
            it->second.last_used = ++m_clock;
            return it->second;
        }

        if (m_frames.size() >= m_capacity)
        {
            evict_one();
        }

        Frame frame;
        if (from_disk)
        {
            frame.data = m_pager.read_page(page_id);
        }

        frame.pin_count = 1;
        frame.dirty = false;
        frame.last_used = ++m_clock;

        auto [inserted_it, ok] = m_frames.emplace(page_id, std::move(frame));
        return inserted_it->second;
    }

    std::vector<char>& BufferManager::fetch_page(uint32_t page_id)
    {
        return get_or_load(page_id, true).data;
    }

    std::vector<char>& BufferManager::new_page(uint32_t page_id)
    {
        return get_or_load(page_id, false).data;
    }

    void BufferManager::unpin_page(uint32_t page_id, bool mark_dirty_flag)
    {
        auto it = m_frames.find(page_id);
        if (it == m_frames.end())
        {
            throw std::runtime_error("unpin page: page not found in buffer pool");
        }

        if (mark_dirty_flag)
        {
            it->second.dirty = true;
        }

        if (it->second.pin_count > 0)
        {
            it->second.pin_count--;
        }
    }

    void BufferManager::flush_page(uint32_t page_id)
    {
        auto it = m_frames.find(page_id);
        if (it == m_frames.end())
        {
            return;
        }

        if (it->second.dirty)
        {
            m_pager.write_page(page_id, it->second.data);
            it->second.dirty = false;
        }
    }

    void BufferManager::flush_all()
    {
        for(auto& [page_id, frame]: m_frames)
        {
            if (frame.dirty)
            {
                m_pager.write_page(page_id, frame.data);
                frame.dirty = false;
            }
        }
    }

    void BufferManager::evict_one()
    {
        uint32_t page_eviction_id = 0;
        bool found = 0;
        uint64_t oldest = std::numeric_limits<uint64_t>::max();

        for (auto& [page_id, frame]: m_frames)
        {
            if (frame.pin_count == 0 && frame.last_used < oldest)
            {
                oldest = frame.last_used;
                page_eviction_id = page_id;
                found = true;
            }
        }

        if (!found)
        {
            throw std::runtime_error("buffer pool eviction: all pages are pinned");
        }

        auto it = m_frames.find(page_eviction_id);
        if (it->second.dirty)
        {
            m_pager.write_page(page_eviction_id, it->second.data);
        }
        m_frames.erase(page_eviction_id);
    }
}
