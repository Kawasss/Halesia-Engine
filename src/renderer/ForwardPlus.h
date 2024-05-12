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
	VkBuffer GetCellBuffer()   { return cellBuffer; }
	VkBuffer GetLightBuffer()  { return lightBuffer; }

private:
	void Allocate();
	void Destroy();
	void CreateShader();

	static constexpr int MAX_LIGHT_INDICES = 7;
	static constexpr int MAX_LIGHTS = 1024;

	// can't use floats here because GLSL / SPIRV padding for a float array is fucked up.
	//
	// GLSL pads this struct like this:
	// - lightCount:       4 bytes
	// - lightIndices[0]:  8 bytes
	// ...
	// - lightIndices[^2]: 8 bytes
	// - lightIndices[^1]: 4 bytes
	//
	// which I simply cannot achieve in normal C++, so I give up.
	// this does waste a lot of memory, because of the padding.
	struct Cell
	{
		alignas(4) float lightCount;
		char lightIndices[(MAX_LIGHT_INDICES - 1) * (sizeof(float) * 2) + sizeof(float)];
	};

	struct Matrices
	{
		glm::mat4 projection;
		glm::mat4 view;
	};

	uint32_t cellWidth = 32, cellHeight = 32;

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