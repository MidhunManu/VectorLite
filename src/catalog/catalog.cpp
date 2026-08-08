#include "catalog.hpp"
#include "catalog/collection_descriptor.hpp"
#include "catalog/schema.hpp"
#include "vectordb/config.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace vectordb
{
	namespace
	{
		void write_u8(std::vector<char>& buf, uint8_t value)
        {
            buf.push_back(static_cast<char>(value));
        }

        void write_u32(std::vector<char>& buf, uint32_t value)
        {
            size_t offset = buf.size();
            buf.resize(offset + sizeof(value));
            std::memcpy(buf.data() + offset, &value, sizeof(value));
        }

        void write_u64(std::vector<char>& buf, uint64_t value)
        {
            size_t offset = buf.size();
            buf.resize(offset + sizeof(value));
            std::memcpy(buf.data() + offset, &value, sizeof(value));
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
            if (cursor + sizeof(uint8_t) > buf.size())
                throw std::runtime_error("catalog buffer truncated (u8)");
            uint8_t value = static_cast<uint8_t>(buf[cursor]);
            cursor += sizeof(uint8_t);
            return value;
        }

        uint32_t read_u32(const std::vector<char>& buf, size_t& cursor)
        {
            if (cursor + sizeof(uint32_t) > buf.size())
                throw std::runtime_error("catalog buffer truncated (u32)");
            uint32_t value;
            std::memcpy(&value, buf.data() + cursor, sizeof(value));
            cursor += sizeof(value);
            return value;
        }

        uint64_t read_u64(const std::vector<char>& buf, size_t& cursor)
        {
            if (cursor + sizeof(uint64_t) > buf.size())
                throw std::runtime_error("catalog buffer truncated (u64)");
            uint64_t value;
            std::memcpy(&value, buf.data() + cursor, sizeof(value));
            cursor += sizeof(value);
            return value;
        }

        std::string read_string(const std::vector<char>& buf, size_t& cursor)
        {
            uint32_t len = read_u32(buf, cursor);
            if (cursor + len > buf.size())
                throw std::runtime_error("catalog buffer truncated (string)");
            std::string value(buf.data() + cursor, len);
            cursor += len;
            return value;
        }
	}

	const CollectionDescriptor& Catalog::create_collection(
		const std::string& name,
		std::vector<ColumnRef> schema)
	{
		if (m_collections.contains(name)) {
			throw std::runtime_error("collection already exists");
		}

		CollectionDescriptor descriptor;

		descriptor.id = m_next_id++;
		descriptor.name = name;
		descriptor.row_count_estimate = 0;
		descriptor.root_page = 0;
		descriptor.schema = std::move(schema);

		auto [it, inserted] = m_collections.emplace(name, std::move(descriptor));
		return  it->second;
	}

	const CollectionDescriptor* Catalog::find_collection(const std::string& name) const
	{
		auto it = m_collections.find(name);
		return it == m_collections.end() ? nullptr : &it->second;
	}

	std::vector<char> Catalog::serialise() const
	{
		std::vector<char> buffer;
		buffer.reserve(PAGE_SIZE);

		write_u32(buffer, static_cast<uint32_t>(m_collections.size()));
		for(const auto [name, descriptor]: m_collections) {
			write_u32(buffer, descriptor.id);
			write_string(buffer, descriptor.name);
			write_u32(buffer, descriptor.root_page);
			write_u64(buffer, descriptor.row_count_estimate);

			write_u32(buffer, static_cast<uint32_t>(descriptor.schema.size()));

			for(const auto& column: descriptor.schema)
			{
				write_string(buffer, column.name);
				write_u8(buffer, static_cast<uint8_t>(column.type));
				write_u32(buffer, column.vector_dim);
				write_u8(buffer, column.nullable ? 1: 0);
			}
		}

		if (buffer.size() > PAGE_SIZE) {
			throw std::runtime_error("catalog exceeds page size");
		}

		buffer.resize(PAGE_SIZE, 0);
		return  buffer;
	}

	Catalog Catalog::deserialise(const std::vector<char>& buffer)
    {
        Catalog catalog;
        size_t cursor = 0;

        uint32_t count = read_u32(buffer, cursor);

        for (uint32_t i = 0; i < count; ++i)
        {
            CollectionDescriptor descriptor;
            descriptor.id = read_u32(buffer, cursor);
            descriptor.name = read_string(buffer, cursor);
            descriptor.root_page = read_u32(buffer, cursor);
            descriptor.row_count_estimate = read_u64(buffer, cursor);

            uint32_t num_columns = read_u32(buffer, cursor);
            descriptor.schema.reserve(num_columns);

            for (uint32_t c = 0; c < num_columns; ++c)
            {
                ColumnRef column;
                column.name = read_string(buffer, cursor);
                column.type = static_cast<ColumnType>(read_u8(buffer, cursor));
                column.vector_dim = read_u32(buffer, cursor);
                column.nullable = read_u8(buffer, cursor) != 0;
                descriptor.schema.push_back(std::move(column));
            }

            catalog.m_next_id = std::max(catalog.m_next_id, descriptor.id + 1);
            catalog.m_collections.emplace(descriptor.name, std::move(descriptor));
        }

        return catalog;
    }
}
