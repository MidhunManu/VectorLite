#pragma once
#include <optional>
#include <utility>
#include <vector>
#include "storage/record.hpp"
#include "storage/slotted_page.hpp"

namespace vectordb
{
    struct Row
    {
        RecordId id;
        Record record;
    };

    class Operator
    {
    public:
        virtual ~Operator() = default;
        virtual void open() = 0;
        virtual std::optional<Row> next() = 0;
        virtual void close() = 0;
    };

    class WriteOperator
    {
    public:
        virtual ~WriteOperator() = default;
        virtual uint64_t run() = 0;
    };

    inline std::vector<Row> drain(Operator& op)
    {
        std::vector<Row> rows;
        op.open();
        try
        {
            while (auto row = op.next()) rows.push_back(std::move(*row));
        }
        catch (...)
        {
            op.close();
            throw;
        }
        op.close();
        return rows;
    }
}
