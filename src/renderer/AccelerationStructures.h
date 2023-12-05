#pragma once
#include <vulkan/vulkan.h>
#include "StorageBuffer.h"

struct Mesh;
struct VulkanCreationObject;
class Object;

class AccelerationStructure // or AS for short
{
public:
	virtual void Destroy();
	VkDeviceAddress GetAccelerationStructureAddress() { return ASAddress; }
	VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;

protected:
	void CreateAS(const VulkanCreationObject& creationObject, const VkAccelerationStructureGeometryKHR* pGeometry, VkAccelerationStructureTypeKHR type, uint32_t maxPrimitiveCount);
	void BuildAS(const VulkanCreationObject& creationObject, const VkAccelerationStructureGeometryKHR* pGeometry, uint32_t primitiveCount, bool useSingleTimeCommands = true, VkCommandBuffer externalCommandBuffer = VK_NULL_HANDLE);

	VkBuffer ASBuffer =              VK_NULL_HANDLE;
	VkDeviceMemory ASBufferMemory =  VK_NULL_HANDLE;

	VkBuffer scratchBuffer =             VK_NULL_HANDLE;
	VkDeviceMemory scratchDeviceMemory = VK_NULL_HANDLE;

	VkDevice logicalDevice = VK_NULL_HANDLE;

	VkDeviceAddress ASAddress = 0;

private:
	VkAccelerationStructureTypeKHR type;
};

class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
	static BottomLevelAccelerationStructure* Create(const VulkanCreationObject& creationObject, Mesh& mesh);
};

class TopLevelAccelerationStructure : public AccelerationStructure
{
public:
	static TopLevelAccelerationStructure* Create(const VulkanCreationObject& creationObject, std::vector<Object*> objects);

	/// <summary>
	/// Builds the top level acceleration structure. It uses single time commands per default, but can use an external command buffer. An external command buffer is recommended if it's being rebuild with performance in mind
	/// </summary>
	void Build(const VulkanCreationObject& creationObject, std::vector<Object*> objects, bool useSingleTimeCommands = true, VkCommandBuffer externalCommandBuffer = VK_NULL_HANDLE);

private:
	static std::vector<VkAccelerationStructureInstanceKHR> GetInstances(std::vector<Object*> objects);

	StorageBuffer<VkAccelerationStructureInstanceKHR> instanceBuffer;
};