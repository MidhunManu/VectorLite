#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "storage/slotted_page.hpp"
#include "storage/page.hpp"

namespace vectordb {
	class BufferManager;
	class Pager;
	class FileHeader;
	class CollectionDescriptor;

	class StorageEngine
	{
	public:
		StorageEngine(BufferManager& buffer_manager, Pager& pager, FileHeader& header);
		RecordId insert_record(CollectionDescriptor& collection, const std::vector<char>& record_bytes);
		std::optional<std::vector<char>> read_record(const RecordId& id);
		bool delete_record(const RecordId& id);
		std::vector<std::pair<RecordId, std::vector<char>>> scan_collection(const CollectionDescriptor& collection);
		uint32_t allocate_page(PageType type);

	private:
		BufferManager& m_buffer_manager;
		Pager& m_pager;
		FileHeader& m_header;
	};
}
