#pragma once
#include <vulkan/vulkan.h>

class ComputeShader;
struct Light;

class ForwardPlusRenderer
{
public:
	ForwardPlusRenderer();
	~ForwardPlusRenderer();

	void Draw(VkCommandBuffer commandBuffer);
	void AddLight(Light& light);

private:
	void Allocate();
	void Destroy();
	void CreateShader();

	static constexpr int MAX_LIGHT_INDICES = 32;

	struct Cell
	{
		uint32_t lightCount;
		uint32_t lightIndices[MAX_LIGHT_INDICES];
	};

	uint32_t cellWidth = 1, cellHeight = 1, cellDepth = 1;

	VkBuffer cellBuffer = VK_NULL_HANDLE;
	VkDeviceMemory cellMemory = VK_NULL_HANDLE;

	ComputeShader* computeShader = nullptr;
};