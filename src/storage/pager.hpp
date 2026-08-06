#pragma once

#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>

namespace vectordb
{
	class Pager
	{
	public:
		explicit Pager(
			const std::string &path);

		~Pager();

		void write_page(
			uint32_t page_id,
			std::vector<char> &data);

		std::vector<char> read_page(
			uint32_t page_id);

	private:
		std::fstream m_file;
	};
}
