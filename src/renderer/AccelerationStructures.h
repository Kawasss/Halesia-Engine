#pragma once
#include <vulkan/vulkan.h>
#include "Buffers.h"
#include "../ResourceManager.h"
#include "../Object.h"

class BottomLevelAccelerationStructure // could be maybe be merged with TopLevelAccelerationStructure for a base class?
{
public:
	static BottomLevelAccelerationStructure* CreateBottomLevelAccelerationStructure(const VulkanCreationObject& creationObject, Mesh& mesh);
	void Build(const VulkanCreationObject& creationObject, const VkAccelerationStructureGeometryKHR* pGeometries, const VkAccelerationStructureBuildSizesInfoKHR& buildSizesInfo, uint32_t faceCount);
	void Destroy();

	VkBuffer buffer;
	VkDeviceMemory deviceMemory;
	VkAccelerationStructureKHR accelerationStructure;

	VkBuffer scratchBuffer;
	VkDeviceMemory scratchDeviceMemory;

	VkDeviceAddress deviceAddress;

private:
	VkDevice logicalDevice;
};

class TopLevelAccelerationStructure
{
public:
	static TopLevelAccelerationStructure* CreateTopLevelAccelerationStructure(const VulkanCreationObject& creationObject, std::vector<BottomLevelAccelerationStructure*> BLAS);
	void Build(const VulkanCreationObject& creationObject, const VkAccelerationStructureGeometryKHR* pGeometry, const VkAccelerationStructureBuildSizesInfoKHR& buildSizesInfo, uint32_t instanceCount);
	void Destroy();

	VkBuffer buffer;
	VkDeviceMemory deviceMemory;
	VkAccelerationStructureKHR accelerationStructure;

	VkBuffer scratchBuffer;
	VkDeviceMemory scratchMemory;

private:
	static ApeironBuffer<VkAccelerationStructureInstanceKHR> instanceBuffer;
	static bool TLASInstancesIsInit;

	VkDevice logicalDevice;
};