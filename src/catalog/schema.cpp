#include "catalog/schema.hpp"
#include <cctype>
#include <unordered_map>

namespace vectordb
{
    namespace
    {
        std::string to_upper(std::string_view s)
        {
            std::string r(s);
            for (char& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return r;
        }
    }

    std::optional<ColumnType> parse_column_type(std::string_view name)
    {
        static const std::unordered_map<std::string, ColumnType> types = {
            {"INT32", ColumnType::Int32}, {"INT64", ColumnType::Int64},
            {"FLOAT32", ColumnType::Float32}, {"STRING", ColumnType::String},
            {"BOOL", ColumnType::Bool},
        };
        auto it = types.find(to_upper(name));
        return it == types.end() ? std::nullopt : std::optional<ColumnType>(it->second);
    }
}