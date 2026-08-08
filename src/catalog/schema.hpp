#pragma once

#include <cstdint>
#include <string>
namespace vectordb
{
	enum class ColumnType: uint8_t
	{
		Int32 	= 0,
		Int65 	= 1,
		Float32 = 2,
		Float64 = 3,
		String 	= 4,
		Bool 	= 5,
		Vector 	= 6	
	};

	struct ColumnRef
	{
		std::string name;
		ColumnType type;
		uint32_t vector_dim = 0; // only to be used if ColumnType is a Vector
		bool nullable = false;
	};
}
