#include "pager.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <stdexcept>

#include "vectordb/config.hpp"

namespace vectordb
{
    Pager::Pager(const std::string &path)
        : m_path(path)
    {
        m_file.open(path, std::ios::in | std::ios::out | std::ios::binary);

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

    std::vector<char> Pager::read_page(uint32_t page_id)
    {
        std::vector<char> buffer(PAGE_SIZE);

        m_file.seekg(static_cast<std::streamoff>(page_id) * PAGE_SIZE);
        m_file.read(buffer.data(), buffer.size());

        return buffer;
    }

    void Pager::write_page(uint32_t page_id, std::vector<char> &data)
    {
        m_file.seekp(static_cast<std::streamoff>(page_id) * PAGE_SIZE);
        m_file.write(data.data(), data.size());
        m_file.flush();
    }

    void Pager::sync()
    {
        m_file.flush();

        // fsync via a second descriptor: flushes the file's dirty pages regardless of which fd wrote them.
        int fd = ::open(m_path.c_str(), O_RDONLY);
        if (fd < 0)
            throw std::runtime_error("cannot open database file for sync");

        int rc = -1;
#ifdef __APPLE__
        rc = ::fcntl(fd, F_FULLFSYNC);
#endif
        if (rc != 0)
            rc = ::fsync(fd);
        ::close(fd);

        if (rc != 0)
            throw std::runtime_error("fsync of database file failed");
    }
}