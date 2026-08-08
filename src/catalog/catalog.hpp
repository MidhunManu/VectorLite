#pragma once

#include "catalog/schema.hpp"
#include "collection_descriptor.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace vectordb
{
	class Catalog
	{
		public:
			Catalog() = default;
			const CollectionDescriptor& create_collection(
				const std::string& name,
				std::vector<ColumnRef> schema
			);
			const CollectionDescriptor* find_collection(const std::string& name) const;
			std::vector<char> serialise() const;
			static Catalog deserialise(const std::vector<char>& buffer);
			
		private:
			std::unordered_map<std::string, CollectionDescriptor> m_collections;
			uint32_t m_next_id = 1;
	};
}
