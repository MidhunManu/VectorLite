#pragma once

#include <cstdint>

namespace vectordb
{
	enum class PageType: uint8_t
	{
		Free 	= 0,
		Header 	= 1,
		Catalog = 2,
		Data 	= 3,
	};
}
