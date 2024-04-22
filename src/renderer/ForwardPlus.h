#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "../glm.h"

class ComputeShader;
class Camera;

class ForwardPlusRenderer
{
public:
	ForwardPlusRenderer();
	~ForwardPlusRenderer();

	void Draw(VkCommandBuffer commandBuffer, Camera* camera);
	void AddLight(glm::vec3 pos);

private:
	void Allocate();
	void Destroy();
	void CreateShader();

	static constexpr int MAX_LIGHT_INDICES = 32;
	static constexpr int MAX_LIGHTS = 1024;

	struct Cell
	{
		uint32_t lightCount;
		uint32_t lightIndices[MAX_LIGHT_INDICES];
	};

	struct Matrices
	{
		glm::mat4 projection;
		glm::mat4 view;
	};

	uint32_t cellWidth = 1, cellHeight = 1, cellDepth = 1;

	VkBuffer cellBuffer = VK_NULL_HANDLE;
	VkDeviceMemory cellMemory = VK_NULL_HANDLE;

	VkBuffer lightBuffer = VK_NULL_HANDLE;
	VkDeviceMemory lightMemory = VK_NULL_HANDLE;

	VkBuffer matricesBuffer = VK_NULL_HANDLE;
	VkDeviceMemory matricesMemory = VK_NULL_HANDLE;

	uint32_t lightCount = 0;

	Matrices* matrices = nullptr;
	ComputeShader* computeShader = nullptr;
};