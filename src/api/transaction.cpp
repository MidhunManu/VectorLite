#include "api/transaction.hpp"
#include "storage/storage_engine.hpp"

namespace vectordb
{
    void Transaction::log_insert(uint32_t collection_id, const RecordId& id, std::vector<char> bytes)
    {
        m_ops.push_back(WalOp{WalRecordType::Insert, collection_id, id.page_id, id.slot_id, std::move(bytes)});
    }

    void Transaction::log_delete(uint32_t collection_id, const RecordId& id)
    {
        m_ops.push_back(WalOp{WalRecordType::Delete, collection_id, id.page_id, id.slot_id, {}});
    }

    void Transaction::rollback_to(size_t mark, StorageEngine& storage)
    {
        // Slots are never reused, so tombstone/un-tombstone is an exact inverse.
        while (m_ops.size() > mark)
        {
            const WalOp& op = m_ops.back();
            RecordId id{op.page_id, op.slot_id};
            if (op.type == WalRecordType::Insert)
                storage.delete_record(id);
            else
                storage.undelete_record(id);
            m_ops.pop_back();
        }
    }
}
