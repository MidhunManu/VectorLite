#include "wal/wal_writer.hpp"
#include "util/byte_io.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

namespace vectordb
{
    WalWriter::WalWriter(const std::string& path)
    {
        // M5: the log starts empty on every open. M7 replaces this with
        // "replay committed records, then truncate".
        m_fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644);
        if (m_fd < 0)
        {
            throw std::runtime_error("cannot open WAL file: " + path + " (" + std::strerror(errno) + ")");
        }
    }

    WalWriter::~WalWriter()
    {
        if (m_fd >= 0) ::close(m_fd);
    }

    void WalWriter::append_record(std::vector<char>& out, uint64_t txn_id,
                                  WalRecordType type, const std::vector<char>& payload)
    {
        size_t start = out.size();
        bytes::put<uint64_t>(out, m_next_lsn++);
        bytes::put<uint64_t>(out, txn_id);
        bytes::put<uint8_t>(out, static_cast<uint8_t>(type));
        bytes::put<uint32_t>(out, static_cast<uint32_t>(payload.size()));
        bytes::put_bytes(out, payload.data(), payload.size());
        uint32_t checksum = bytes::fnv1a(out.data() + start, out.size() - start);
        bytes::put<uint32_t>(out, checksum);
    }

    void WalWriter::write_all(const std::vector<char>& data)
    {
        size_t done = 0;
        while (done < data.size())
        {
            ssize_t n = ::write(m_fd, data.data() + done, data.size() - done);
            if (n < 0)
            {
                if (errno == EINTR) continue;
                throw std::runtime_error(std::string("WAL write failed: ") + std::strerror(errno));
            }
            if (n == 0)
            {
                throw std::runtime_error("WAL write made no progress"); // avoid spinning forever
            }
            done += static_cast<size_t>(n);
        }
    }

    void WalWriter::sync()
    {
#ifdef __APPLE__
        // On macOS, fsync() does not flush the drive's cache; F_FULLFSYNC does.
        if (::fcntl(m_fd, F_FULLFSYNC) == 0) return;
#endif
        if (::fsync(m_fd) != 0)
        {
            throw std::runtime_error(std::string("WAL fsync failed: ") + std::strerror(errno));
        }
    }

    void WalWriter::commit(uint64_t txn_id, const std::vector<WalOp>& ops)
    {
        std::vector<char> body;
        append_record(body, txn_id, WalRecordType::Begin, {});

        for (const WalOp& op : ops)
        {
            std::vector<char> payload;
            bytes::put<uint32_t>(payload, op.collection_id);
            bytes::put<uint32_t>(payload, op.page_id);
            bytes::put<uint16_t>(payload, op.slot_id);
            if (op.type == WalRecordType::Insert)
            {
                bytes::put<uint32_t>(payload, static_cast<uint32_t>(op.bytes.size()));
                bytes::put_bytes(payload, op.bytes.data(), op.bytes.size());
            }
            append_record(body, txn_id, op.type, payload);
        }

        write_all(body);
        sync();

        std::vector<char> commit_record;
        append_record(commit_record, txn_id, WalRecordType::Commit, {});
        write_all(commit_record);
        sync();
    }

    void WalWriter::reset()
    {
        if (::ftruncate(m_fd, 0) != 0)
        {
            throw std::runtime_error(std::string("WAL truncate failed: ") + std::strerror(errno));
        }
        sync();
    }
}
