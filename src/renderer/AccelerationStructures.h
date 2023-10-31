#pragma once
#include <vulkan/vulkan.h>
#include "../ResourceManager.h"

struct Mesh;
struct VulkanCreationObject;
class Object;

class AccelerationStructure // AS for short
{
protected:
	void Create(const VulkanCreationObject& creationObject, const VkAccelerationStructureGeometryKHR* pGeometry, uint32_t maxPrimitiveCount);
	void Build(const VulkanCreationObject& creationObject);

	VkAccelerationStructureKHR accelerationStructure;
	VkBuffer ASBuffer;
	VkDeviceMemory ASBufferMemory;

	VkBuffer scratchBuffer;
	VkDeviceMemory scratchDeviceMemory;

	VkDeviceAddress ASAddress;
};

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
	static TopLevelAccelerationStructure* CreateTopLevelAccelerationStructure(const VulkanCreationObject& creationObject, std::vector<Object*> objects);

	/// <summary>
	/// Builds the top level acceleration structure. It uses single time commands per default, but can use an external command buffer. An external command buffer is recommended if it's being rebuild with performance in mind
	/// </summary>
	void Build(const VulkanCreationObject& creationObject, std::vector<Object*> objects, bool useSingleTimeCommands = true, VkCommandBuffer externalCommandBuffer = VK_NULL_HANDLE);
	void Destroy();

	VkBuffer buffer;
	VkDeviceMemory deviceMemory;
	VkAccelerationStructureKHR accelerationStructure;

	VkBuffer scratchBuffer = VK_NULL_HANDLE;
	VkDeviceMemory scratchMemory = VK_NULL_HANDLE;

private:
	static std::vector<VkAccelerationStructureInstanceKHR> GetInstances(std::vector<Object*> objects);

	ApeironBuffer<VkAccelerationStructureInstanceKHR> instanceBuffer;
	VkDevice logicalDevice;
};