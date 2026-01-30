module;

#include "system/CriticalSection.h"
module Renderer.TLAS;

import <vulkan/vulkan.h>;

import std;

import Renderer;
import Renderer.Vulkan;

TopLevelAccelerationStructure::TopLevelAccelerationStructure()
{
	instanceBuffer.Reserve(Renderer::MAX_TLAS_INSTANCES, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geometry.geometry.instances.arrayOfPointers = VK_FALSE;
	geometry.geometry.instances.data = { Vulkan::GetDeviceAddress(instanceBuffer.GetBufferHandle()) };

	CreateAS(&geometry, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, Renderer::MAX_TLAS_INSTANCES);
}

TopLevelAccelerationStructure* TopLevelAccelerationStructure::Create()
{
	TopLevelAccelerationStructure* TLAS = new TopLevelAccelerationStructure();
	return TLAS;
}

void TopLevelAccelerationStructure::Build(const std::vector<RenderableMesh>& objects, InstanceIndexType indexType, VkCommandBuffer externalCommandBuffer)
{
	instanceBuffer.Reset();
	std::vector<VkAccelerationStructureInstanceKHR> BLASInstances = GetInstances(objects, indexType); // write all of the BLAS instances to a single buffer so that vulkan can easily read all of the instances in one go
	instanceBuffer.SubmitNewData(BLASInstances);										              // make it so that only the new BLASs get submitted instead of all of the BLASs (even the old ones). the code right now is a REALLY bad implementation

	VkAccelerationStructureGeometryKHR geometry{};
	GetGeometry(geometry);

	BuildAS(&geometry, (uint32_t)BLASInstances.size(), VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, externalCommandBuffer);
	hasBeenBuilt = true;
}

void TopLevelAccelerationStructure::Update(const std::vector<RenderableMesh>& objects, InstanceIndexType indexType, VkCommandBuffer externalCommandBuffer)
{
	instanceBuffer.Reset();
	std::vector<VkAccelerationStructureInstanceKHR> BLASInstances = GetInstances(objects, indexType);
	instanceBuffer.SubmitNewData(BLASInstances);

	VkAccelerationStructureGeometryKHR geometry{};
	GetGeometry(geometry);

	BuildAS(&geometry, (uint32_t)BLASInstances.size(), VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, externalCommandBuffer);
}

void TopLevelAccelerationStructure::GetGeometry(VkAccelerationStructureGeometryKHR& geometry)
{
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geometry.geometry.instances.arrayOfPointers = VK_FALSE;
	geometry.geometry.instances.data = { Vulkan::GetDeviceAddress(instanceBuffer.GetBufferHandle()) };
}

std::vector<VkAccelerationStructureInstanceKHR> TopLevelAccelerationStructure::GetInstances(const std::vector<RenderableMesh>& objects, InstanceIndexType indexType)
{
	uint32_t processedAmount = 0; // add a second counter for each processed mesh. if an object is checked, but it doesnt have a mesh it will leave an empty instance custom index, which results in data missalignment
	std::vector<VkAccelerationStructureInstanceKHR> instances;
	instances.reserve(objects.size()); // can over allocate
	for (int i = 0; i < objects.size(); i++)
	{
		const RenderableMesh& mesh = objects[i];
		if (mesh.ShouldBeNotRayTraced()) // objects marked STATUS_INVISIBLE or STATUS_DISABLED shouldn't be rendered
			continue;

		VkAccelerationStructureInstanceKHR instance{};
		instance.instanceCustomIndex = indexType == InstanceIndexType::Identifier ? processedAmount : mesh.materialIndex;
		instance.mask = 0xFF;

		if (mesh.ShouldNotCull())
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

		instance.accelerationStructureReference = mesh.BLAS->GetAccelerationStructureAddress();

		glm::mat4 transform = glm::transpose(mesh.transform);
		memcpy(&instance.transform, &transform, sizeof(VkTransformMatrixKHR));											   // simply copy the contents of the glm matrix to the vulkan matrix since the contents align

		instances.push_back(instance);
		processedAmount++;
	}
	return instances;
}

bool TopLevelAccelerationStructure::HasBeenBuilt() const
{
	return hasBeenBuilt;
}