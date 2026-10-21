#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "wal/wal_record.hpp"

namespace vectordb
{
    // On-disk record: [lsn u64][txn_id u64][type u8][payload_len u32][payload][checksum u32]
    // (payload_len added to the spec envelope so records are parseable.)
    class WalWriter
    {
    public:
        explicit WalWriter(const std::string& path);
        ~WalWriter();
        WalWriter(const WalWriter&) = delete;
        WalWriter& operator=(const WalWriter&) = delete;

        // Durable on return: BEGIN + ops fsync'd, then COMMIT fsync'd.
        void commit(uint64_t txn_id, const std::vector<WalOp>& ops);

        // Checkpoint: truncate the log (caller must have synced the main file).
        void reset();

    private:
        int m_fd = -1;
        uint64_t m_next_lsn = 1;

        void append_record(std::vector<char>& out, uint64_t txn_id,
                           WalRecordType type, const std::vector<char>& payload);
        void write_all(const std::vector<char>& data);
        void sync();
    };
}
