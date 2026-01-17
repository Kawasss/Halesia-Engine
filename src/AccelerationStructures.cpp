module;

#include "renderer/Vulkan.h"
#include "renderer/Mesh.h"
#include "renderer/VulkanAPIError.h"

#include "core/MeshObject.h"

module Renderer.AccelerationStructure;

import <vulkan/vulkan.h>;

import std;

import Renderer.VulkanGarbageManager;
import Renderer;

constexpr VkBufferUsageFlags ACCELERATION_STRUCTURE_BUFFER_BITS = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
constexpr VkBufferUsageFlags SCRATCH_BUFFER_BITS = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

static VkAccelerationStructureGeometryTrianglesDataKHR GetTrianglesData(const Mesh& mesh)
{
	VkDeviceAddress vertexBufferAddress = Vulkan::GetDeviceAddress(Renderer::g_vertexBuffer.GetBufferHandle());
	VkDeviceAddress indexBufferAddress = Vulkan::GetDeviceAddress(Renderer::g_indexBuffer.GetBufferHandle());

	VkAccelerationStructureGeometryTrianglesDataKHR data{};
	data.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;

	data.vertexData = { vertexBufferAddress + Renderer::g_vertexBuffer.GetMemoryOffset(mesh.vertexMemory) };
	data.indexData  = { indexBufferAddress  + Renderer::g_indexBuffer.GetMemoryOffset(mesh.indexMemory)   };

	data.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	data.vertexStride = sizeof(Vertex);
	data.maxVertex = static_cast<uint32_t>(mesh.vertices.size());
	data.indexType = VK_INDEX_TYPE_UINT32;
	
	data.transformData = { 0 };

	return data;
}

void AccelerationStructure::CreateAS(const VkAccelerationStructureGeometryKHR* pGeometry, VkAccelerationStructureTypeKHR type, uint32_t maxPrimitiveCount)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	this->type = type;

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
	buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR;
	buildGeometryInfo.type = type;
	buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	buildGeometryInfo.geometryCount = 1;
	buildGeometryInfo.pGeometries = pGeometry;
	buildGeometryInfo.scratchData = { 0 };

	VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
	buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	vkGetAccelerationStructureBuildSizesKHR(ctx.logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &maxPrimitiveCount, &buildSizesInfo);

	size       = buildSizesInfo.accelerationStructureSize;
	buildSize  = buildSizesInfo.buildScratchSize;
	UpdateSize = buildSizesInfo.updateScratchSize;

	ASBuffer.Init(size, ACCELERATION_STRUCTURE_BUFFER_BITS, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkAccelerationStructureCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.type = type;
	createInfo.size = size;
	createInfo.buffer = ASBuffer.Get();
	createInfo.offset = 0;

	VkResult result = vkCreateAccelerationStructureKHR(ctx.logicalDevice, &createInfo, nullptr, &accelerationStructure);
	CheckVulkanResult("Failed to create an acceleration structure", result);

	if (type == VkAccelerationStructureTypeKHR::VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
		Vulkan::SetDebugName(accelerationStructure, "top-level");
	else
		Vulkan::SetDebugName(accelerationStructure, "bottom-level");

	ASAddress = Vulkan::GetDeviceAddress(accelerationStructure);

	scratchBuffer.Init(buildSize, SCRATCH_BUFFER_BITS, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void AccelerationStructure::BuildAS(const VkAccelerationStructureGeometryKHR* pGeometry, uint32_t primitiveCount, VkBuildAccelerationStructureModeKHR mode, VkCommandBuffer externalCommandBuffer)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
	buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR;
	buildGeometryInfo.mode = mode;
	buildGeometryInfo.type = type;
	buildGeometryInfo.srcAccelerationStructure = accelerationStructure;
	buildGeometryInfo.dstAccelerationStructure = accelerationStructure;
	buildGeometryInfo.geometryCount = 1;
	buildGeometryInfo.pGeometries = pGeometry;
	buildGeometryInfo.scratchData = { Vulkan::GetDeviceAddress(scratchBuffer.Get()) };

	VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
	buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	vkGetAccelerationStructureBuildSizesKHR(ctx.logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &primitiveCount, &buildSizesInfo);

	if (buildSize < buildSizesInfo.buildScratchSize)
		return; // build request too big

	VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
	buildRangeInfo.primitiveCount = primitiveCount;
	const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;

	if (externalCommandBuffer == VK_NULL_HANDLE)
	{
		Vulkan::ExecuteSingleTimeCommands([&](const CommandBuffer& cmdBuffer)
			{
				vkCmdBuildAccelerationStructuresKHR(cmdBuffer.Get(), 1, &buildGeometryInfo, &pBuildRangeInfo);
			}
		);
	}
	else // this option is faster for runtime building since it doesn't wait for the queue to go idle (which can be a long time)
	{
		VkMemoryBarrier memoryBarrier{};
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

		vkCmdBuildAccelerationStructuresKHR(externalCommandBuffer, 1, &buildGeometryInfo, &pBuildRangeInfo);
		vkCmdPipelineBarrier(externalCommandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	}
}

AccelerationStructure::~AccelerationStructure()
{
	vgm::Delete(accelerationStructure);
}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(const Mesh& mesh)
{
	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.triangles = GetTrianglesData(mesh);

	CreateAS(&geometry, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, mesh.faceCount * 10);
	BuildAS(&geometry, mesh.faceCount, VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR);
}

BottomLevelAccelerationStructure* BottomLevelAccelerationStructure::Create(const Mesh& mesh)
{
	BottomLevelAccelerationStructure* BLAS = new BottomLevelAccelerationStructure(mesh);
	return BLAS;
}

void BottomLevelAccelerationStructure::RebuildGeometry(VkCommandBuffer commandBuffer, const Mesh& mesh)
{
	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.triangles = GetTrianglesData(mesh);

	BuildAS(&geometry, mesh.faceCount, VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, commandBuffer);
}

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

void TopLevelAccelerationStructure::Build(const std::vector<MeshObject*>& objects, InstanceIndexType indexType, VkCommandBuffer externalCommandBuffer)
{
	instanceBuffer.Reset();
	std::vector<VkAccelerationStructureInstanceKHR> BLASInstances = GetInstances(objects, indexType); // write all of the BLAS instances to a single buffer so that vulkan can easily read all of the instances in one go
	instanceBuffer.SubmitNewData(BLASInstances);										              // make it so that only the new BLASs get submitted instead of all of the BLASs (even the old ones). the code right now is a REALLY bad implementation

	VkAccelerationStructureGeometryKHR geometry{};
	GetGeometry(geometry);

	BuildAS(&geometry, (uint32_t)BLASInstances.size(), VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, externalCommandBuffer);
	hasBeenBuilt = true;
}

void TopLevelAccelerationStructure::Update(const std::vector<MeshObject*>& objects, InstanceIndexType indexType, VkCommandBuffer externalCommandBuffer)
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

std::vector<VkAccelerationStructureInstanceKHR> TopLevelAccelerationStructure::GetInstances(const std::vector<MeshObject*>& objects, InstanceIndexType indexType)
{
	uint32_t processedAmount = 0; // add a second counter for each processed mesh. if an object is checked, but it doesnt have a mesh it will leave an empty instance custom index, which results in data missalignment
	std::vector<VkAccelerationStructureInstanceKHR> instances;
	instances.reserve(objects.size()); // can over allocate
	for (int i = 0; i < objects.size(); i++)
	{
		win32::CriticalLockGuard lockGuard(objects[i]->GetCriticalSection());
		if (!objects[i]->HasFinishedLoading() || objects[i]->state != OBJECT_STATE_VISIBLE || !objects[i]->mesh.IsValid() || !objects[i]->mesh.CanBeRayTraced()) // objects marked STATUS_INVISIBLE or STATUS_DISABLED shouldn't be rendered
			continue;

		VkAccelerationStructureInstanceKHR instance{};
		instance.instanceCustomIndex = indexType == InstanceIndexType::Identifier ? processedAmount : objects[i]->mesh.GetMaterialIndex();
		instance.mask = 0xFF;

		if (!objects[i]->mesh.cullBackFaces)
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

		instance.accelerationStructureReference = objects[i]->mesh.BLAS->GetAccelerationStructureAddress();
			
		glm::mat4 transform = glm::transpose(objects[i]->transform.GetModelMatrix());
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