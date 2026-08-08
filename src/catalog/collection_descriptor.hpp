#pragma once

#include "schema.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace vectordb
{
	struct CollectionDescriptor
	{
		uint32_t id = 0;
		std::string name;
		uint32_t root_page = 0;
		uint64_t page_size_estimate = 0;
		std::vector<ColumnRef> schema;
	};
}
