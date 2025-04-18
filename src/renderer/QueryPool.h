#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <map>

#include "CommandBuffer.h"

class QueryPool
{
public:
	QueryPool() = default;
	QueryPool(const QueryPool&)       = delete;
	QueryPool& operator=(QueryPool&&) = delete;

	void Create(VkQueryType type, uint32_t amount);
	void WriteTimeStamp(CommandBuffer commandBuffer);
	void Reset(CommandBuffer commandBuffer);
	void Fetch();

	void BeginTimestamp(CommandBuffer commandBuffer, const std::string& label);
	void EndTimestamp(CommandBuffer commandBuffer, const std::string& label);

	std::map<std::string, uint64_t> GetTimestamps() const;

	uint64_t& operator[](size_t index);

	~QueryPool();
	void Destroy();

private:
	struct Timestamp
	{
		uint64_t* begin;
		uint64_t* end;
	};

	uint32_t size = 0;
	VkQueryType queryType = VK_QUERY_TYPE_MAX_ENUM;
	VkQueryPool pool = VK_NULL_HANDLE;
	uint64_t* data = nullptr;

	std::map<std::string, Timestamp> timestamps;

	uint32_t timestampIndex = 0;
};