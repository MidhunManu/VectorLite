#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include "storage/slotted_page.hpp"

namespace vectordb
{
    class StorageEngine;

    // Implemented by Transaction. Lets exec/ record mutations without
    // depending on the api/ layer.
    class MutationSink
    {
    public:
        virtual ~MutationSink() = default;
        virtual void log_insert(uint32_t collection_id, const RecordId& id, std::vector<char> bytes) = 0;
        virtual void log_delete(uint32_t collection_id, const RecordId& id) = 0;
    };

    struct ExecContext
    {
        StorageEngine& storage;
        MutationSink& sink;
        std::function<void()> on_catalog_changed; // e.g. first insert allocated a root page
    };
}
