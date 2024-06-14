#pragma once
#include <vulkan/vulkan.h>

class QueryPool
{
public:
	QueryPool() = default;
	QueryPool(const QueryPool&)       = delete;
	QueryPool& operator=(QueryPool&&) = delete;

	void Create(VkQueryType type, uint32_t amount);
	void WriteTimeStamp(VkCommandBuffer commandBuffer);
	void Reset(VkCommandBuffer commandBuffer);
	void Fetch();

	uint64_t& operator[](size_t index);

	~QueryPool();

private:
	void Destroy();

	uint32_t size = 0;
	VkQueryType queryType = VK_QUERY_TYPE_MAX_ENUM;
	VkQueryPool pool = VK_NULL_HANDLE;
	uint64_t* data = nullptr;

	uint32_t timeStampIndex = 0;
};