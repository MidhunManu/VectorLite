#include "pager.hpp"
#include "stdexcept"
#include "vectordb/config.hpp"

namespace vectordb
{
    Pager::Pager(const std::string &path)
    {
        m_file.open(
            path,
            std::ios::in |
            std::ios::out |
            std::ios::binary
        );

        if (!m_file)
        {
            throw std::runtime_error("cannot open database file");
        }
    }

    Pager::~Pager()
    {
        if (m_file.is_open())
        {
            m_file.close();
        }
    }

    std::vector<char>& Pager::read_page(uint32_t page_id)
    {
        std::vector<char> buffer(PAGE_SIZE);
        m_file.read(buffer.data(), buffer.size());

        return buffer;
    }

    void Pager::write_page(uint32_t page_id, std::vector<char>& data)
    {
        m_file.seekp(
            page_id* PAGE_SIZE
        );

        m_file.write(
            data.data(),
            data.size()
        );

        m_file.flush();
    }
}
