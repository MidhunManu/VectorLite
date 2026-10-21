#pragma once

#include <stdexcept>
#include <string>
#include <cstddef>
#include <utility>

namespace vectordb {
	class QueryError: public std::runtime_error
	{
	public:
		QueryError(std::string message, size_t offset)
			: std::runtime_error(std::move(message)),
			m_offset(offset)
		{}

		size_t offset() const
		{
			return m_offset;
		}

	private:
		size_t m_offset;
	};
}
