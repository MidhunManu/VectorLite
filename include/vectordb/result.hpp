#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "storage/record.hpp"

namespace vectordb
{
    struct ResultSet
    {
        std::vector<std::string> columns;
        std::vector<std::vector<Value>> rows;
        uint64_t affected_rows = 0;
    };
}
