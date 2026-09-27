#include "storage/record.hpp"

#include <cstring>
#include <stdexcept>

namespace vectordb
{
    Value Value::null() { return Value{std::monostate{}}; }
    Value Value::from_int32(int32_t v) { return Value{v}; }
    Value Value::from_int64(int64_t v) { return Value{v}; }
    Value Value::from_float32(float v) { return Value{v}; }
    Value Value::from_string(std::string v) { return Value{std::move(v)}; }
    Value Value::from_bool(bool v) { return Value{v}; }

    bool Value::is_null() const
    {
        return std::holds_alternative<std::monostate>(data);
    }

    ValueType Value::type() const
    {
        if (std::holds_alternative<std::monostate>(data)) return ValueType::Null;
        if (std::holds_alternative<int32_t>(data)) return ValueType::Int32;
        if (std::holds_alternative<int64_t>(data)) return ValueType::Int64;
        if (std::holds_alternative<float>(data)) return ValueType::Float32;
        if (std::holds_alternative<std::string>(data)) return ValueType::String;
        if (std::holds_alternative<bool>(data)) return ValueType::Bool;
        throw std::runtime_error("unreachable: unknown Value variant state");
    }

    Record::Record(std::vector<Value> values) : m_values(std::move(values)) {}

    const std::vector<Value>& Record::values() const { return m_values; }
    size_t Record::size() const { return m_values.size(); }

    const Value& Record::at(size_t index) const
    {
        return m_values.at(index);
    }

    namespace
    {
        void write_u32(std::vector<char>& buf, uint32_t value)
        {
            size_t offset = buf.size();
            buf.resize(offset + sizeof(value));
            std::memcpy(buf.data() + offset, &value, sizeof(value));
        }

        void write_u8(std::vector<char>& buf, uint8_t value)
        {
            buf.push_back(static_cast<char>(value));
        }

        template <typename T>
        void write_pod(std::vector<char>& buf, const T& value)
        {
            size_t offset = buf.size();
            buf.resize(offset + sizeof(T));
            std::memcpy(buf.data() + offset, &value, sizeof(T));
        }

        void write_string(std::vector<char>& buf, const std::string& value)
        {
            write_u32(buf, static_cast<uint32_t>(value.size()));
            size_t offset = buf.size();
            buf.resize(offset + value.size());
            std::memcpy(buf.data() + offset, value.data(), value.size());
        }

        uint8_t read_u8(const std::vector<char>& buf, size_t& cursor)
        {
            if (cursor + 1 > buf.size()) throw std::runtime_error("record buffer truncated (u8)");
            uint8_t v = static_cast<uint8_t>(buf[cursor]);
            cursor += 1;
            return v;
        }

        uint32_t read_u32(const std::vector<char>& buf, size_t& cursor)
        {
            if (cursor + 4 > buf.size()) throw std::runtime_error("record buffer truncated (u32)");
            uint32_t v;
            std::memcpy(&v, buf.data() + cursor, 4);
            cursor += 4;
            return v;
        }

        template <typename T>
        T read_pod(const std::vector<char>& buf, size_t& cursor)
        {
            if (cursor + sizeof(T) > buf.size()) throw std::runtime_error("record buffer truncated (pod)");
            T v;
            std::memcpy(&v, buf.data() + cursor, sizeof(T));
            cursor += sizeof(T);
            return v;
        }

        std::string read_string(const std::vector<char>& buf, size_t& cursor)
        {
            uint32_t len = read_u32(buf, cursor);
            if (cursor + len > buf.size()) throw std::runtime_error("record buffer truncated (string)");
            std::string v(buf.data() + cursor, len);
            cursor += len;
            return v;
        }
    }

    std::vector<char> Record::serialise() const
    {
        std::vector<char> buffer;
        write_u32(buffer, static_cast<uint32_t>(m_values.size()));

        for (const auto& value : m_values)
        {
            write_u8(buffer, static_cast<uint8_t>(value.type()));

            switch (value.type())
            {
                case ValueType::Null:
                    break;
                case ValueType::Int32:
                    write_pod(buffer, std::get<int32_t>(value.data));
                    break;
                case ValueType::Int64:
                    write_pod(buffer, std::get<int64_t>(value.data));
                    break;
                case ValueType::Float32:
                    write_pod(buffer, std::get<float>(value.data));
                    break;
                case ValueType::String:
                    write_string(buffer, std::get<std::string>(value.data));
                    break;
                case ValueType::Bool:
                    write_u8(buffer, std::get<bool>(value.data) ? 1 : 0);
                    break;
            }
        }

        return buffer;
    }

    Record Record::deserialise(const std::vector<char>& buffer)
    {
        size_t cursor = 0;
        uint32_t count = read_u32(buffer, cursor);

        std::vector<Value> values;
        values.reserve(count);

        for (uint32_t i = 0; i < count; ++i)
        {
            ValueType type = static_cast<ValueType>(read_u8(buffer, cursor));

            switch (type)
            {
                case ValueType::Null:
                    values.push_back(Value::null());
                    break;
                case ValueType::Int32:
                    values.push_back(Value::from_int32(read_pod<int32_t>(buffer, cursor)));
                    break;
                case ValueType::Int64:
                    values.push_back(Value::from_int64(read_pod<int64_t>(buffer, cursor)));
                    break;
                case ValueType::Float32:
                    values.push_back(Value::from_float32(read_pod<float>(buffer, cursor)));
                    break;
                case ValueType::String:
                    values.push_back(Value::from_string(read_string(buffer, cursor)));
                    break;
                case ValueType::Bool:
                    values.push_back(Value::from_bool(read_u8(buffer, cursor) != 0));
                    break;
                default:
                    throw std::runtime_error("record buffer: unknown value type tag");
            }
        }

        return Record(std::move(values));
    }
}