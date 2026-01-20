module;

#include "CommandBuffer.h"

export module Renderer.QueryPool;

import <vulkan/vulkan.h>;

import std;

export class QueryPool
{
public:
	QueryPool() = default;
	QueryPool(const QueryPool&) = delete;
	QueryPool& operator=(QueryPool&&) = delete;

	void Create(VkQueryType type, std::uint32_t amount);
	void WriteTimeStamp(const CommandBuffer& commandBuffer);
	void Reset(const CommandBuffer& commandBuffer);
	void Fetch();

	void BeginTimestamp(const CommandBuffer& commandBuffer, const std::string& label);
	void EndTimestamp(const CommandBuffer& commandBuffer, const std::string& label);

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

	std::uint32_t size = 0;
	VkQueryType queryType = VK_QUERY_TYPE_MAX_ENUM;
	VkQueryPool pool = VK_NULL_HANDLE;
	uint64_t* data = nullptr;

	std::map<std::string, Timestamp> timestamps;

	std::uint32_t timestampIndex = 0;
};