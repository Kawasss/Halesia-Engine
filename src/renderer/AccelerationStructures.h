#pragma once
#include <vulkan/vulkan.h>
#include "StorageBuffer.h"

struct Mesh;
class Object;

class AccelerationStructure // or AS for short
{
public:
	virtual void Destroy();
	VkDeviceAddress GetAccelerationStructureAddress() { return ASAddress; }
	VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;

protected:
	void CreateAS(const VkAccelerationStructureGeometryKHR* pGeometry, VkAccelerationStructureTypeKHR type, uint32_t maxPrimitiveCount);
	void BuildAS(const VkAccelerationStructureGeometryKHR* pGeometry, uint32_t primitiveCount, VkBuildAccelerationStructureModeKHR mode, bool useSingleTimeCommands = true, VkCommandBuffer externalCommandBuffer = VK_NULL_HANDLE);

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
	static BottomLevelAccelerationStructure* Create(Mesh& mesh);
};

class TopLevelAccelerationStructure : public AccelerationStructure
{
public:
	static TopLevelAccelerationStructure* Create(std::vector<Object*>& objects);

	/// <summary>
	/// Builds the top level acceleration structure. It uses single time commands per default, but can use an external command buffer. An external command buffer is recommended if it's being rebuild with performance in mind
	/// </summary>
	void Build(std::vector<Object*>& objects, bool useSingleTimeCommands = true, VkCommandBuffer externalCommandBuffer = VK_NULL_HANDLE);
	void Update(std::vector<Object*>& objects, VkCommandBuffer externalCommandBuffer);
	bool HasBeenBuilt();

private:
	static std::vector<VkAccelerationStructureInstanceKHR> GetInstances(std::vector<Object*>& objects);
	void GetGeometry(VkAccelerationStructureGeometryKHR& geometry);

	StorageBuffer<VkAccelerationStructureInstanceKHR> instanceBuffer;
	bool hasBeenBuilt = false;
};