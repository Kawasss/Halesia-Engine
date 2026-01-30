export module Renderer.TLAS;

import <vulkan/vulkan.h>;

import std;

import Renderer.AccelerationStructure;
import Renderer.StorageBuffer;
import Renderer.RenderableMesh;

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
	void Build(const std::vector<RenderableMesh>& objects, InstanceIndexType indexType, VkCommandBuffer externalCommandBuffer = VK_NULL_HANDLE);
	void Update(const std::vector<RenderableMesh>& objects, InstanceIndexType indexType, VkCommandBuffer externalCommandBuffer);
	bool HasBeenBuilt() const;

private:
	static std::vector<VkAccelerationStructureInstanceKHR> GetInstances(const std::vector<RenderableMesh>& objects, InstanceIndexType indexType);
	void GetGeometry(VkAccelerationStructureGeometryKHR& geometry);

	StorageBuffer<VkAccelerationStructureInstanceKHR> instanceBuffer;
	bool hasBeenBuilt = false;
};