#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace vectordb
{
    enum class ValueType : uint8_t
    {
        Null = 0,
        Int32 = 1,
        Int64 = 2,
        Float32 = 3,
        String = 4,
        Bool = 5,
    };

    using ValueVariant = std::variant<std::monostate, int32_t, int64_t, float, std::string, bool>;

    struct Value
    {
        ValueVariant data;

        static Value null();
        static Value from_int32(int32_t v);
        static Value from_int64(int64_t v);
        static Value from_float32(float v);
        static Value from_string(std::string v);
        static Value from_bool(bool v);

        bool is_null() const;
        ValueType type() const;
    };

    class Record
    {
    public:
        Record() = default;
        explicit Record(std::vector<Value> values);

        const std::vector<Value>& values() const;
        size_t size() const;
        const Value& at(size_t index) const;

        std::vector<char> serialise() const;
        static Record deserialise(const std::vector<char>& buffer);

    private:
        std::vector<Value> m_values;
    };
}