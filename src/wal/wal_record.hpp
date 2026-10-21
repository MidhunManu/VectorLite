#pragma once
#include <cstdint>
#include <vector>

namespace vectordb
{
    enum class WalRecordType : uint8_t
    {
        Begin = 1, Insert = 2, Delete = 3, Update = 4, Commit = 5, Rollback = 6
    };

    // One logged row mutation, held in memory by a Transaction until commit.
    // Carries the exact (page, slot) so M7's redo can be made idempotent.
    struct WalOp
    {
        WalRecordType type;        // Insert or Delete in M5
        uint32_t collection_id;
        uint32_t page_id;
        uint16_t slot_id;
        std::vector<char> bytes;   // serialised record; empty for Delete
    };
}
