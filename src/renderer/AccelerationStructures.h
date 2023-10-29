#pragma once
#include <vulkan/vulkan.h>
#include "Buffers.h"

class BottomLevelAccelerationStructure
{
public:
	static BottomLevelAccelerationStructure* CreateBottomLevelAccelerationStructure(const VulkanCreationObject& creationObject, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer, uint32_t vertexSize, uint32_t faceCount);
	void Destroy();

	VkBuffer buffer;
	VkDeviceMemory deviceMemory;
	VkAccelerationStructureKHR accelerationStructure;

	VkBuffer scratchBuffer;
	VkDeviceMemory scratchDeviceMemory;

	VkBuffer geometryInstanceBuffer;
	VkDeviceMemory geometryInstanceBufferMemory;

	VkDeviceAddress deviceAddress;

private:
	VkDevice logicalDevice;
};

class TopLevelAccelerationStructure
{
public:
	static TopLevelAccelerationStructure* CreateTopLevelAccelerationStructure(const VulkanCreationObject& creationObject, BottomLevelAccelerationStructure* BLAS);
	void Destroy();

	VkBuffer buffer;
	VkDeviceMemory deviceMemory;
	VkAccelerationStructureKHR accelerationStructure;

	VkBuffer scratchBuffer;
	VkDeviceMemory scratchMemory;

private:
	VkDevice logicalDevice;
};