module;

#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "Mesh.h"

#include "../core/MeshObject.h"

export module Renderer.AccelerationStructure;

import std;

import Renderer.StorageBuffer;

export class AccelerationStructure // or AS for short
{
public:
	VkDeviceAddress GetAccelerationStructureAddress() { return ASAddress; }
	VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;

	~AccelerationStructure();

protected:
	void CreateAS(const VkAccelerationStructureGeometryKHR* pGeometry, VkAccelerationStructureTypeKHR type, uint32_t maxPrimitiveCount);
	void BuildAS(const VkAccelerationStructureGeometryKHR* pGeometry, uint32_t primitiveCount, VkBuildAccelerationStructureModeKHR mode, VkCommandBuffer externalCommandBuffer = VK_NULL_HANDLE);

	Buffer ASBuffer;
	Buffer scratchBuffer;

	VkDeviceSize size = 0;
	VkDeviceSize buildSize = 0;
	VkDeviceSize UpdateSize = 0;

	VkDeviceAddress ASAddress = 0;

private:
	VkAccelerationStructureTypeKHR type = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
};

export class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
	BottomLevelAccelerationStructure(const Mesh& mesh);

	static BottomLevelAccelerationStructure* Create(const Mesh& mesh);
	void RebuildGeometry(VkCommandBuffer commandBuffer, const Mesh& mesh);
};

export class TopLevelAccelerationStructure : public AccelerationStructure
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
	void Build(const std::vector<MeshObject*>& objects, InstanceIndexType indexType, VkCommandBuffer externalCommandBuffer = VK_NULL_HANDLE);
	void Update(const std::vector<MeshObject*>& objects, InstanceIndexType indexType, VkCommandBuffer externalCommandBuffer);
	bool HasBeenBuilt() const;

private:
	static std::vector<VkAccelerationStructureInstanceKHR> GetInstances(const std::vector<MeshObject*>& objects, InstanceIndexType indexType);
	void GetGeometry(VkAccelerationStructureGeometryKHR& geometry);

	StorageBuffer<VkAccelerationStructureInstanceKHR> instanceBuffer;
	bool hasBeenBuilt = false;
};