#pragma once
#include <vulkan/vulkan.h>

#include "StorageBuffer.h"
#include "Buffer.h"

struct Mesh;
class Object;

class AccelerationStructure // or AS for short
{
public:
	virtual void Destroy();
	VkDeviceAddress GetAccelerationStructureAddress() { return ASAddress; }
	VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;

	~AccelerationStructure() { Destroy(); }

protected:
	void CreateAS(const VkAccelerationStructureGeometryKHR* pGeometry, VkAccelerationStructureTypeKHR type, uint32_t maxPrimitiveCount);
	void BuildAS(const VkAccelerationStructureGeometryKHR* pGeometry, uint32_t primitiveCount, VkBuildAccelerationStructureModeKHR mode, VkCommandBuffer externalCommandBuffer = VK_NULL_HANDLE);

	Buffer ASBuffer;
	Buffer scratchBuffer;

	VkDeviceAddress ASAddress = 0;

private:
	VkAccelerationStructureTypeKHR type = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
};

class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
	BottomLevelAccelerationStructure(Mesh& mesh);

	static BottomLevelAccelerationStructure* Create(Mesh& mesh);
	void RebuildGeometry(VkCommandBuffer commandBuffer, Mesh& mesh);
};

class TopLevelAccelerationStructure : public AccelerationStructure
{
public:
	enum class InstanceIndexType
	{
		Identifier, // uses an unique identifier as an instances custom index
		Material,   // uses the object meshes material index as the custom index
	};

	TopLevelAccelerationStructure();

	static TopLevelAccelerationStructure* Create();

	/// <summary>
	/// Builds the top level acceleration structure. It uses single time commands per default, but can use an external command buffer. An external command buffer is recommended if it's being rebuild with performance in mind
	/// </summary>
	void Build(const std::vector<Object*>& objects, InstanceIndexType indexType, VkCommandBuffer externalCommandBuffer = VK_NULL_HANDLE);
	void Update(const std::vector<Object*>& objects, InstanceIndexType indexType, VkCommandBuffer externalCommandBuffer);
	bool HasBeenBuilt();

private:
	static std::vector<VkAccelerationStructureInstanceKHR> GetInstances(const std::vector<Object*>& objects, InstanceIndexType indexType);
	void GetGeometry(VkAccelerationStructureGeometryKHR& geometry);

	StorageBuffer<VkAccelerationStructureInstanceKHR> instanceBuffer;
	bool hasBeenBuilt = false;
};