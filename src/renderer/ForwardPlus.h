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

	static constexpr int MAX_LIGHT_INDICES = 32;
	static constexpr int MAX_LIGHTS = 1024;

	struct Cell
	{
		float lightCount;
		alignas(sizeof(float) * MAX_LIGHT_INDICES) float lightIndices[MAX_LIGHT_INDICES];
	};

	struct Matrices
	{
		glm::mat4 projection;
		glm::mat4 view;
	};

	uint32_t cellWidth = 8, cellHeight = 8, cellDepth = 8;

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