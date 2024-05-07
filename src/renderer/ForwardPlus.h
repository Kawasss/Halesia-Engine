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

	ComputeShader* GetShader() { return computeShader; }
	VkBuffer GetCellBuffer() { return cellBuffer; }

private:
	void Allocate();
	void Destroy();
	void CreateShader();

	static constexpr int MAX_LIGHT_INDICES = 7;
	static constexpr int MAX_LIGHTS = 1024;

	struct Cell
	{
		float lightCount;
		float lightIndices[MAX_LIGHT_INDICES];
	};

	struct Matrices
	{
		glm::mat4 projection;
		glm::mat4 view;
	};

	uint32_t cellWidth = 2, cellHeight = 2, cellDepth = 2;

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