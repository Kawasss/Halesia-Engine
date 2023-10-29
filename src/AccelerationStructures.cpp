#include "renderer/AccelerationStructures.h"

BottomLevelAccelerationStructure* BottomLevelAccelerationStructure::CreateBottomLevelAccelerationStructure(const VulkanCreationObject& creationObject, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer, uint32_t vertexSize, uint32_t faceCount)
{
	BottomLevelAccelerationStructure* BLAS = new BottomLevelAccelerationStructure();
	BLAS->logicalDevice = creationObject.logicalDevice;

	VkCommandPool commandPool = Vulkan::FetchNewCommandPool(creationObject);

	VkDeviceAddress vertexBufferAddress = Vulkan::GetDeviceAddress(creationObject.logicalDevice, vertexBuffer.GetVkBuffer());
	VkDeviceAddress indexBufferAddress = Vulkan::GetDeviceAddress(creationObject.logicalDevice, indexBuffer.GetVkBuffer());

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData = { vertexBufferAddress };
	triangles.vertexStride = sizeof(Vertex);//sizeof(float) * 3;
	triangles.maxVertex = vertexSize;
	triangles.indexType = VK_INDEX_TYPE_UINT16;
	triangles.indexData = { indexBufferAddress };
	triangles.transformData = { 0 };

	VkAccelerationStructureGeometryDataKHR BLASGeometryData{};
	BLASGeometryData.triangles = triangles;

	VkAccelerationStructureGeometryKHR BLASGeometry{};
	BLASGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	BLASGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	BLASGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	BLASGeometry.geometry = BLASGeometryData;

	VkAccelerationStructureBuildGeometryInfoKHR BLASBuildGeometryInfo{};
	BLASBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	BLASBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	BLASBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	BLASBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	BLASBuildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	BLASBuildGeometryInfo.geometryCount = 1;
	BLASBuildGeometryInfo.pGeometries = &BLASGeometry;
	BLASBuildGeometryInfo.scratchData = { 0 };

	VkAccelerationStructureBuildSizesInfoKHR BLASBuildSizesInfo{};
	BLASBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	std::vector<uint32_t> BLMaxPrimitiveCounts = { faceCount };
	vkGetAccelerationStructureBuildSizesKHR(creationObject.logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &BLASBuildGeometryInfo, BLMaxPrimitiveCounts.data(), &BLASBuildSizesInfo);

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, BLASBuildSizesInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, BLAS->buffer, BLAS->deviceMemory);

	VkAccelerationStructureCreateInfoKHR BLASCreateInfo{};
	BLASCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	BLASCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	BLASCreateInfo.size = BLASBuildSizesInfo.accelerationStructureSize;
	BLASCreateInfo.buffer = BLAS->buffer;
	BLASCreateInfo.offset = 0;

	VkResult result = vkCreateAccelerationStructureKHR(creationObject.logicalDevice, &BLASCreateInfo, nullptr, &BLAS->accelerationStructure);
	CheckVulkanResult("Failed to create the BLAS", result, nameof(vkCreateAccelerationStructureKHR));

	// build BLAS

	VkAccelerationStructureDeviceAddressInfoKHR BLASAddressInfo{};
	BLASAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	BLASAddressInfo.accelerationStructure = BLAS->accelerationStructure;

	BLAS->deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(creationObject.logicalDevice, &BLASAddressInfo);

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, BLASBuildSizesInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, BLAS->scratchBuffer, BLAS->scratchDeviceMemory);

	VkDeviceAddress scratchDeviceAddress = Vulkan::GetDeviceAddress(creationObject.logicalDevice, BLAS->scratchBuffer);

	BLASBuildGeometryInfo.scratchData = { scratchDeviceAddress };
	BLASBuildGeometryInfo.dstAccelerationStructure = BLAS->accelerationStructure;

	VkAccelerationStructureBuildRangeInfoKHR BLASBuildRangeInfo{};
	BLASBuildRangeInfo.primitiveCount = faceCount;
	const VkAccelerationStructureBuildRangeInfoKHR* pBLASBuildRangeInfo = &BLASBuildRangeInfo;

	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(creationObject.logicalDevice, commandPool);
	vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &BLASBuildGeometryInfo, &pBLASBuildRangeInfo);
	Vulkan::EndSingleTimeCommands(creationObject.logicalDevice, creationObject.queue, commandBuffer, commandPool);

	Vulkan::YieldCommandPool(creationObject.queueIndex, commandPool);

	return BLAS;
}

void BottomLevelAccelerationStructure::Destroy()
{
	vkFreeMemory(logicalDevice, geometryInstanceBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, geometryInstanceBuffer, nullptr);

	vkDestroyAccelerationStructureKHR(logicalDevice, accelerationStructure, nullptr);

	vkDestroyBuffer(logicalDevice, scratchBuffer, nullptr);
	vkFreeMemory(logicalDevice, scratchDeviceMemory, nullptr);

	vkDestroyBuffer(logicalDevice, buffer, nullptr);
	vkFreeMemory(logicalDevice, deviceMemory, nullptr);
}

TopLevelAccelerationStructure* TopLevelAccelerationStructure::CreateTopLevelAccelerationStructure(const VulkanCreationObject& creationObject, BottomLevelAccelerationStructure* BLAS)
{
	TopLevelAccelerationStructure* TLAS = new TopLevelAccelerationStructure();
	TLAS->logicalDevice = creationObject.logicalDevice;

	VkCommandPool commandPool = Vulkan::FetchNewCommandPool(creationObject);

	VkAccelerationStructureInstanceKHR BLASInstance{};
	BLASInstance.transform = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
	BLASInstance.instanceCustomIndex = 0;
	BLASInstance.mask = 0xFF;
	BLASInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	BLASInstance.accelerationStructureReference = BLAS->deviceAddress;

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, sizeof(VkAccelerationStructureInstanceKHR), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, BLAS->geometryInstanceBuffer, BLAS->geometryInstanceBufferMemory);

	void* BLGeometryInstanceBufferMemPtr;

	VkResult result = vkMapMemory(creationObject.logicalDevice, BLAS->geometryInstanceBufferMemory, 0, sizeof(VkAccelerationStructureInstanceKHR), 0, &BLGeometryInstanceBufferMemPtr);
	CheckVulkanResult("Failed to map the memory of the bottom level geometry instance buffer", result, nameof(vkMapMemory));

	memcpy(BLGeometryInstanceBufferMemPtr, &BLASInstance, sizeof(VkAccelerationStructureInstanceKHR));
	vkUnmapMemory(creationObject.logicalDevice, BLAS->geometryInstanceBufferMemory);

	VkDeviceAddress BLGeometryInstanceAddress = Vulkan::GetDeviceAddress(creationObject.logicalDevice, BLAS->geometryInstanceBuffer);

	VkAccelerationStructureGeometryInstancesDataKHR instances{};
	instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instances.arrayOfPointers = VK_FALSE;
	instances.data = { BLGeometryInstanceAddress };

	VkAccelerationStructureGeometryDataKHR TLASGeometryData{};
	TLASGeometryData.instances = instances;

	VkAccelerationStructureGeometryKHR TLASGeometry{};
	TLASGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	TLASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	TLASGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	TLASGeometry.geometry = TLASGeometryData;

	VkAccelerationStructureBuildGeometryInfoKHR TLASBuildGeometryInfo{};
	TLASBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	TLASBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	TLASBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	TLASBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	TLASBuildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	TLASBuildGeometryInfo.geometryCount = 1;
	TLASBuildGeometryInfo.pGeometries = &TLASGeometry;
	TLASBuildGeometryInfo.scratchData = { 0 };

	VkAccelerationStructureBuildSizesInfoKHR TLASBuildSizesInfo{};
	TLASBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	TLASBuildSizesInfo.accelerationStructureSize = 0;
	TLASBuildSizesInfo.updateScratchSize = 0;
	TLASBuildSizesInfo.buildScratchSize = 0;

	std::vector<uint32_t> TLASMaxPrimitiveCounts{ 1 };

	vkGetAccelerationStructureBuildSizesKHR(creationObject.logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &TLASBuildGeometryInfo, TLASMaxPrimitiveCounts.data(), &TLASBuildSizesInfo);

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, TLASBuildSizesInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TLAS->buffer, TLAS->deviceMemory);

	VkAccelerationStructureCreateInfoKHR TLASCreateInfo{};
	TLASCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	TLASCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	TLASCreateInfo.size = TLASBuildSizesInfo.accelerationStructureSize;
	TLASCreateInfo.buffer = TLAS->buffer;
	TLASCreateInfo.deviceAddress = 0;

	result = vkCreateAccelerationStructureKHR(creationObject.logicalDevice, &TLASCreateInfo, nullptr, &TLAS->accelerationStructure);
	CheckVulkanResult("Failed to create the TLAS", result, nameof(vkCreateAccelerationStructureKHR));

	// build TLAS

	VkAccelerationStructureDeviceAddressInfoKHR TLASAddressInfo{};
	TLASAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	TLASAddressInfo.accelerationStructure = TLAS->accelerationStructure;

	VkDeviceAddress TLASDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(creationObject.logicalDevice, &TLASAddressInfo);

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, TLASBuildSizesInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TLAS->scratchBuffer, TLAS->scratchMemory);

	VkDeviceAddress TLASScratchBufferAddress = Vulkan::GetDeviceAddress(creationObject.logicalDevice, TLAS->scratchBuffer);

	TLASBuildGeometryInfo.dstAccelerationStructure = TLAS->accelerationStructure;
	TLASBuildGeometryInfo.scratchData = { TLASScratchBufferAddress };

	VkAccelerationStructureBuildRangeInfoKHR TLASBuildRangeInfo{};
	TLASBuildRangeInfo.primitiveCount = 1;
	const VkAccelerationStructureBuildRangeInfoKHR* pTLASBuildRangeInfos = &TLASBuildRangeInfo;

	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(creationObject.logicalDevice, commandPool);
	vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &TLASBuildGeometryInfo, &pTLASBuildRangeInfos);
	Vulkan::EndSingleTimeCommands(creationObject.logicalDevice, creationObject.queue, commandBuffer, commandPool);

	Vulkan::YieldCommandPool(creationObject.queueIndex, commandPool);

	return TLAS;
}

void TopLevelAccelerationStructure::Destroy()
{
	vkFreeMemory(logicalDevice, scratchMemory, nullptr);
	vkDestroyBuffer(logicalDevice, scratchBuffer, nullptr);

	vkDestroyAccelerationStructureKHR(logicalDevice, accelerationStructure, nullptr);

	vkFreeMemory(logicalDevice, deviceMemory, nullptr);
	vkDestroyBuffer(logicalDevice, buffer, nullptr);
}