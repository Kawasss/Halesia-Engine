#include "renderer/AccelerationStructures.h"
#include "renderer/Renderer.h"

BottomLevelAccelerationStructure* BottomLevelAccelerationStructure::CreateBottomLevelAccelerationStructure(const VulkanCreationObject& creationObject, Mesh& mesh)
{
	BottomLevelAccelerationStructure* BLAS = new BottomLevelAccelerationStructure();
	BLAS->logicalDevice = creationObject.logicalDevice;

	VkDeviceAddress vertexBufferAddress = Vulkan::GetDeviceAddress(creationObject.logicalDevice, Renderer::globalVertexBuffer.GetBufferHandle());
	VkDeviceAddress indexBufferAddress = Vulkan::GetDeviceAddress(creationObject.logicalDevice, Renderer::globalIndicesBuffer.GetBufferHandle());

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	geometry.geometry.triangles.vertexData = { vertexBufferAddress + Renderer::globalVertexBuffer.GetMemoryOffset(mesh.vertexMemory) };
	geometry.geometry.triangles.vertexStride = sizeof(Vertex);
	geometry.geometry.triangles.maxVertex = mesh.vertices.size();
	geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT16;
	geometry.geometry.triangles.indexData = { indexBufferAddress + Renderer::globalIndicesBuffer.GetMemoryOffset(mesh.indexMemory) };
	geometry.geometry.triangles.transformData = { 0 };

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
	buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	buildGeometryInfo.geometryCount = 1;
	buildGeometryInfo.pGeometries = &geometry;
	buildGeometryInfo.scratchData = { 0 };

	VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
	buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	std::vector<uint32_t> maxPrimitiveCounts = { (uint32_t)mesh.faceCount };
	vkGetAccelerationStructureBuildSizesKHR(creationObject.logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, maxPrimitiveCounts.data(), &buildSizesInfo);

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, buildSizesInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, BLAS->buffer, BLAS->deviceMemory);

	VkAccelerationStructureCreateInfoKHR BLASCreateInfo{};
	BLASCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	BLASCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	BLASCreateInfo.size = buildSizesInfo.accelerationStructureSize;
	BLASCreateInfo.buffer = BLAS->buffer;
	BLASCreateInfo.offset = 0;

	VkResult result = vkCreateAccelerationStructureKHR(creationObject.logicalDevice, &BLASCreateInfo, nullptr, &BLAS->accelerationStructure);
	CheckVulkanResult("Failed to create the BLAS", result, nameof(vkCreateAccelerationStructureKHR));

	BLAS->deviceAddress = Vulkan::GetDeviceAddress(creationObject.logicalDevice, BLAS->accelerationStructure);

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, buildSizesInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, BLAS->scratchBuffer, BLAS->scratchDeviceMemory);

	BLAS->Build(creationObject, &geometry, buildSizesInfo, mesh.faceCount);

	return BLAS;
}

void BottomLevelAccelerationStructure::Build(const VulkanCreationObject& creationObject, const VkAccelerationStructureGeometryKHR* pGeometries, const VkAccelerationStructureBuildSizesInfoKHR& buildSizesInfo, uint32_t faceCount)
{
	VkCommandPool commandPool = Vulkan::FetchNewCommandPool(creationObject);

	VkAccelerationStructureBuildGeometryInfoKHR BLASBuildGeometryInfo{};
	BLASBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	BLASBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	BLASBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	BLASBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	BLASBuildGeometryInfo.dstAccelerationStructure = accelerationStructure;
	BLASBuildGeometryInfo.geometryCount = 1;
	BLASBuildGeometryInfo.pGeometries = pGeometries;
	BLASBuildGeometryInfo.scratchData = { Vulkan::GetDeviceAddress(creationObject.logicalDevice, scratchBuffer) };

	VkAccelerationStructureBuildRangeInfoKHR BLASBuildRangeInfo{};
	BLASBuildRangeInfo.primitiveCount = faceCount;
	const VkAccelerationStructureBuildRangeInfoKHR* pBLASBuildRangeInfo = &BLASBuildRangeInfo;

	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(creationObject.logicalDevice, commandPool);
	vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &BLASBuildGeometryInfo, &pBLASBuildRangeInfo);
	Vulkan::EndSingleTimeCommands(creationObject.logicalDevice, creationObject.queue, commandBuffer, commandPool);

	Vulkan::YieldCommandPool(creationObject.queueIndex, commandPool);
}

void BottomLevelAccelerationStructure::Destroy()
{
	vkDestroyAccelerationStructureKHR(logicalDevice, accelerationStructure, nullptr);

	vkDestroyBuffer(logicalDevice, scratchBuffer, nullptr);
	vkFreeMemory(logicalDevice, scratchDeviceMemory, nullptr);

	vkDestroyBuffer(logicalDevice, buffer, nullptr);
	vkFreeMemory(logicalDevice, deviceMemory, nullptr);
}

std::vector<VkAccelerationStructureInstanceKHR> TopLevelAccelerationStructure::GetInstances(std::vector<Object*> objects)
{
	std::vector<VkAccelerationStructureInstanceKHR> instances;
	for (int i = 0; i < objects.size(); i++)
	{
		for (int j = 0; j < objects[i]->meshes.size(); j++)
		{
			VkAccelerationStructureInstanceKHR instance{};
			instance.instanceCustomIndex = j;
			instance.mask = 0xFF;
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			instance.accelerationStructureReference = objects[i]->meshes[j].BLAS->deviceAddress;

			glm::mat4 transform = objects[i]->transform.GetModelMatrix();
			memcpy(&instance.transform, &transform, sizeof(VkTransformMatrixKHR));

			instances.push_back(instance);
		}
	}
	return instances;
}

TopLevelAccelerationStructure* TopLevelAccelerationStructure::CreateTopLevelAccelerationStructure(const VulkanCreationObject& creationObject, std::vector<Object*> objects)
{
	TopLevelAccelerationStructure* TLAS = new TopLevelAccelerationStructure();
	TLAS->logicalDevice = creationObject.logicalDevice;
	TLAS->instanceBuffer.Reserve(creationObject, Renderer::MAX_TLAS_INSTANCES, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geometry.geometry.instances.arrayOfPointers = VK_FALSE;
	geometry.geometry.instances.data = { Vulkan::GetDeviceAddress(creationObject.logicalDevice, TLAS->instanceBuffer.GetBufferHandle()) };

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
	buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	buildGeometryInfo.geometryCount = 1; // must be 1 according to the spec
	buildGeometryInfo.pGeometries = &geometry;
	buildGeometryInfo.scratchData = { 0 };

	VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
	buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	vkGetAccelerationStructureBuildSizesKHR(creationObject.logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &Renderer::MAX_TLAS_INSTANCES, &buildSizesInfo);

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, buildSizesInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TLAS->buffer, TLAS->deviceMemory);

	VkAccelerationStructureCreateInfoKHR TLASCreateInfo{};
	TLASCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	TLASCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	TLASCreateInfo.size = buildSizesInfo.accelerationStructureSize;
	TLASCreateInfo.buffer = TLAS->buffer;
	TLASCreateInfo.deviceAddress = 0;

	VkResult result = vkCreateAccelerationStructureKHR(creationObject.logicalDevice, &TLASCreateInfo, nullptr, &TLAS->accelerationStructure);
	CheckVulkanResult("Failed to create the TLAS", result, nameof(vkCreateAccelerationStructureKHR));

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, buildSizesInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TLAS->scratchBuffer, TLAS->scratchMemory);

	TLAS->Build(creationObject, objects);
	
	return TLAS;
}

void TopLevelAccelerationStructure::Build(const VulkanCreationObject& creationObject, std::vector<Object*> objects, bool useSingleTimeCommands, VkCommandBuffer externalCommandBuffer)
{
	if (!useSingleTimeCommands && externalCommandBuffer == VK_NULL_HANDLE)
		throw VulkanAPIError("Can't build the top level acceleration structure, because no valid command buffer was given (!useSingleTimeCommands && externalCommandBuffer == VK_NULL_HANDLE)", VK_SUCCESS, nameof(Build), __FILENAME__, __STRLINE__);

	instanceBuffer.ResetAddressPointer();

	std::vector<VkAccelerationStructureInstanceKHR> BLASInstances = GetInstances(objects);
	instanceBuffer.SubmitNewData(BLASInstances); // make it so that only the new BLASs get submitted instead of all of the BLASs (even the old ones). the code right now is a REALLY bad implementation

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geometry.geometry.instances.arrayOfPointers = VK_FALSE;
	geometry.geometry.instances.data = { Vulkan::GetDeviceAddress(creationObject.logicalDevice, instanceBuffer.GetBufferHandle()) };

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
	buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildGeometryInfo.dstAccelerationStructure = accelerationStructure;
	buildGeometryInfo.pGeometries = &geometry;
	buildGeometryInfo.geometryCount = 1; // must be 1 according to the spec
	buildGeometryInfo.scratchData = { Vulkan::GetDeviceAddress(creationObject.logicalDevice, scratchBuffer) };

	VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
	buildRangeInfo.primitiveCount = static_cast<uint32_t>(BLASInstances.size());
	const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfos = &buildRangeInfo;

	if (useSingleTimeCommands)
	{
		VkCommandPool commandPool = Vulkan::FetchNewCommandPool(creationObject);

		VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(creationObject.logicalDevice, commandPool);
		vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeometryInfo, &pBuildRangeInfos);
		Vulkan::EndSingleTimeCommands(creationObject.logicalDevice, creationObject.queue, commandBuffer, commandPool);

		Vulkan::YieldCommandPool(creationObject.queueIndex, commandPool);
	}
	else
	{
		VkMemoryBarrier memoryBarrier{};
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

		vkCmdBuildAccelerationStructuresKHR(externalCommandBuffer, 1, &buildGeometryInfo, &pBuildRangeInfos);
		vkCmdPipelineBarrier(externalCommandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	}
}

void TopLevelAccelerationStructure::Destroy()
{
	vkFreeMemory(logicalDevice, scratchMemory, nullptr);
	vkDestroyBuffer(logicalDevice, scratchBuffer, nullptr);

	vkDestroyAccelerationStructureKHR(logicalDevice, accelerationStructure, nullptr);

	vkFreeMemory(logicalDevice, deviceMemory, nullptr);
	vkDestroyBuffer(logicalDevice, buffer, nullptr);
}