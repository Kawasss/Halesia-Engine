#include "renderer/AccelerationStructures.h"
#include "renderer/Renderer.h"
#include "renderer/Mesh.h"
#include "Object.h"

void AccelerationStructure::CreateAS(const VulkanCreationObject& creationObject, const VkAccelerationStructureGeometryKHR* pGeometry, VkAccelerationStructureTypeKHR type, uint32_t maxPrimitiveCount)
{
	this->logicalDevice = creationObject.logicalDevice;
	this->type = type;

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
	buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildGeometryInfo.type = type;
	buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	buildGeometryInfo.geometryCount = 1;
	buildGeometryInfo.pGeometries = pGeometry;
	buildGeometryInfo.scratchData = { 0 };

	VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
	buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	vkGetAccelerationStructureBuildSizesKHR(creationObject.logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &maxPrimitiveCount, &buildSizesInfo);

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, buildSizesInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ASBuffer, ASBufferMemory);

	VkAccelerationStructureCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.type = type;
	createInfo.size = buildSizesInfo.accelerationStructureSize;
	createInfo.buffer = ASBuffer;
	createInfo.offset = 0;

	VkResult result = vkCreateAccelerationStructureKHR(creationObject.logicalDevice, &createInfo, nullptr, &accelerationStructure);
	CheckVulkanResult((std::string)"Failed to create an acceleration structure (" + string_VkAccelerationStructureTypeKHR(type) + ")", result, nameof(vkCreateAccelerationStructureKHR));

	ASAddress = Vulkan::GetDeviceAddress(creationObject.logicalDevice, accelerationStructure);

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, buildSizesInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchDeviceMemory);
}

void AccelerationStructure::BuildAS(const VulkanCreationObject& creationObject, const VkAccelerationStructureGeometryKHR* pGeometry, uint32_t primitiveCount, bool useSingleTimeCommands, VkCommandBuffer externalCommandBuffer)
{
	if (!useSingleTimeCommands && externalCommandBuffer == VK_NULL_HANDLE) // check if the external command buffer is valid, if specified that an external will be used
		throw VulkanAPIError("Can't build the acceleration structure, because no valid command buffer was given (!useSingleTimeCommands && externalCommandBuffer == VK_NULL_HANDLE)", VK_SUCCESS, nameof(Build), __FILENAME__, __STRLINE__);

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
	buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildGeometryInfo.type = type;
	buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildGeometryInfo.dstAccelerationStructure = accelerationStructure;
	buildGeometryInfo.geometryCount = 1;
	buildGeometryInfo.pGeometries = pGeometry;
	buildGeometryInfo.scratchData = { Vulkan::GetDeviceAddress(creationObject.logicalDevice, scratchBuffer) };

	VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
	buildRangeInfo.primitiveCount = primitiveCount;
	const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;

	if (useSingleTimeCommands)
	{
		VkCommandPool commandPool = Vulkan::FetchNewCommandPool(creationObject);

		VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(creationObject.logicalDevice, commandPool);
		vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeometryInfo, &pBuildRangeInfo);
		Vulkan::EndSingleTimeCommands(creationObject.logicalDevice, creationObject.queue, commandBuffer, commandPool);

		Vulkan::YieldCommandPool(creationObject.queueIndex, commandPool);
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

void AccelerationStructure::Destroy()
{
	vkDestroyAccelerationStructureKHR(logicalDevice, accelerationStructure, nullptr);

	vkDestroyBuffer(logicalDevice, ASBuffer, nullptr);
	vkFreeMemory(logicalDevice, ASBufferMemory, nullptr);

	vkDestroyBuffer(logicalDevice, scratchBuffer, nullptr);
	vkFreeMemory(logicalDevice, scratchDeviceMemory, nullptr);
}

BottomLevelAccelerationStructure* BottomLevelAccelerationStructure::Create(const VulkanCreationObject& creationObject, Mesh& mesh)
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
	geometry.geometry.triangles.maxVertex = static_cast<uint32_t>(mesh.vertices.size());
	geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT16;
	geometry.geometry.triangles.indexData = { indexBufferAddress + Renderer::globalIndicesBuffer.GetMemoryOffset(mesh.indexMemory) };
	geometry.geometry.triangles.transformData = { 0 };

	BLAS->CreateAS(creationObject, &geometry, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, mesh.faceCount);

	BLAS->BuildAS(creationObject, &geometry, mesh.faceCount);

	return BLAS;
}

TopLevelAccelerationStructure* TopLevelAccelerationStructure::Create(const VulkanCreationObject& creationObject, std::vector<Object*> objects)
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

	TLAS->CreateAS(creationObject, &geometry, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, Renderer::MAX_TLAS_INSTANCES);

	if (objects.size() > 0)
		TLAS->Build(creationObject, objects);
	
	return TLAS;
}

void TopLevelAccelerationStructure::Build(const VulkanCreationObject& creationObject, std::vector<Object*> objects, bool useSingleTimeCommands, VkCommandBuffer externalCommandBuffer)
{
	if (!useSingleTimeCommands && externalCommandBuffer == VK_NULL_HANDLE)				   // check if the external command buffer is valid, if specified that an external will be used
		throw VulkanAPIError("Can't build the top level acceleration structure, because no valid command buffer was given (!useSingleTimeCommands && externalCommandBuffer == VK_NULL_HANDLE)", VK_SUCCESS, nameof(Build), __FILENAME__, __STRLINE__);

	instanceBuffer.ResetAddressPointer();

	std::vector<VkAccelerationStructureInstanceKHR> BLASInstances = GetInstances(objects); // write all of the BLAS instances to a single buffer so that vulkan can easily read all of the instances in one go
	instanceBuffer.SubmitNewData(BLASInstances);										   // make it so that only the new BLASs get submitted instead of all of the BLASs (even the old ones). the code right now is a REALLY bad implementation

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geometry.geometry.instances.arrayOfPointers = VK_FALSE;
	geometry.geometry.instances.data = { Vulkan::GetDeviceAddress(creationObject.logicalDevice, instanceBuffer.GetBufferHandle()) };

	BuildAS(creationObject, &geometry, (uint32_t)BLASInstances.size(), useSingleTimeCommands, externalCommandBuffer);
}

std::vector<VkAccelerationStructureInstanceKHR> TopLevelAccelerationStructure::GetInstances(std::vector<Object*> objects)
{
	std::vector<VkAccelerationStructureInstanceKHR> instances;
	for (int i = 0; i < objects.size(); i++)
	{
		if (!objects[i]->HasFinishedLoading() || objects[i]->state == STATUS_INVISIBLE || objects[i]->state == STATUS_DISABLED) // objects marked STATUS_INVISIBLE or STATUS_DISABLED shouldn't be rendered
			continue;

		for (int j = 0; j < objects[i]->meshes.size(); j++)																	   // converts every mesh from every object into an acceleration structure instance
		{
			VkAccelerationStructureInstanceKHR instance{};
			instance.instanceCustomIndex = objects[i]->meshes.size() * i + j;
			instance.mask = 0xFF;
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			instance.accelerationStructureReference = objects[i]->meshes[j].BLAS->GetAccelerationStructureAddress();
			
			glm::mat4 transform = glm::transpose(objects[i]->transform.GetModelMatrix());
			memcpy(&instance.transform, &transform, sizeof(VkTransformMatrixKHR));											   // simply copy the contents of the glm matrix to the vulkan matrix since the contents align

			instances.push_back(instance);
		}
	}
	return instances;
}