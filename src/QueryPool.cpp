#include <cassert>

#include "renderer/QueryPool.h"
#include "renderer/Vulkan.h"

void QueryPool::Create(VkQueryType type, uint32_t amount)
{
	if (data) Destroy();

	queryType = type;
	size = amount;

	data = new uint64_t[amount];
	pool = Vulkan::CreateQueryPool(type, amount);
}

void QueryPool::Destroy()
{
	if (pool == VK_NULL_HANDLE)
		return;

	const Vulkan::Context& ctx = Vulkan::GetContext();
	delete[] data;
	vkDestroyQueryPool(ctx.logicalDevice, pool, nullptr);
	pool = VK_NULL_HANDLE;
}

QueryPool::~QueryPool()
{
	Destroy();
}

void QueryPool::WriteTimeStamp(VkCommandBuffer commandBuffer)
{
	assert(queryType == VK_QUERY_TYPE_TIMESTAMP);

	VkPipelineStageFlagBits stage = timestampIndex % 2 == 0 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	vkCmdWriteTimestamp(commandBuffer, stage, pool, timestampIndex);
	timestampIndex++;
}

void QueryPool::BeginTimestamp(VkCommandBuffer commandBuffer, const std::string& label)
{
	assert(queryType == VK_QUERY_TYPE_TIMESTAMP);

	VkPipelineStageFlagBits stage = timestampIndex % 2 == 0 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	vkCmdWriteTimestamp(commandBuffer, stage, pool, timestampIndex);

	Timestamp& timestamp = timestamps[label];
	timestamp.begin = &data[timestampIndex++];
}

void QueryPool::EndTimestamp(VkCommandBuffer commandBuffer, const std::string& label)
{
	assert(queryType == VK_QUERY_TYPE_TIMESTAMP);

	VkPipelineStageFlagBits stage = timestampIndex % 2 == 0 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	vkCmdWriteTimestamp(commandBuffer, stage, pool, timestampIndex);

	Timestamp& timestamp = timestamps[label];
	timestamp.end = &data[timestampIndex++];
}

std::map<std::string, uint64_t> QueryPool::GetTimestamps()
{
	std::map<std::string, uint64_t> ret;
	for (const auto& [label, timestamp] : timestamps)
	{
		if (timestamp.begin == nullptr || timestamp.end == nullptr) // this defines an inactive timestamp
			continue;

		ret[label] = *timestamp.end - *timestamp.begin;
	}
	return ret;
}

void QueryPool::Fetch()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	vkGetQueryPoolResults(ctx.logicalDevice, pool, 0, size, size * sizeof(uint64_t), data, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
}

void QueryPool::Reset(VkCommandBuffer commandBuffer)
{
	switch (queryType)
	{
	case VK_QUERY_TYPE_TIMESTAMP:
		timestampIndex = 0;
		break;
	}
	vkCmdResetQueryPool(commandBuffer, pool, 0, size);
}

uint64_t& QueryPool::operator[](size_t index)
{
	assert(index < size);
	return data[index];
}