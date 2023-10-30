#include "renderer/AccelerationStructures.h"
#include "renderer/Renderer.h"

constexpr uint32_t MAX_PRIMITIVES = 1000U; // better to use the renderer.cpp variables

bool TopLevelAccelerationStructure::TLASInstancesIsInit = false;
ApeironBuffer<VkAccelerationStructureInstanceKHR> TopLevelAccelerationStructure::instanceBuffer;

BottomLevelAccelerationStructure* BottomLevelAccelerationStructure::CreateBottomLevelAccelerationStructure(const VulkanCreationObject& creationObject, Mesh& mesh)
{
	BottomLevelAccelerationStructure* BLAS = new BottomLevelAccelerationStructure();
	BLAS->logicalDevice = creationObject.logicalDevice;

	VkCommandPool commandPool = Vulkan::FetchNewCommandPool(creationObject);

	VkDeviceAddress vertexBufferAddress = Vulkan::GetDeviceAddress(creationObject.logicalDevice, Renderer::globalVertexBuffer.GetBufferHandle());
	VkDeviceAddress indexBufferAddress = Vulkan::GetDeviceAddress(creationObject.logicalDevice, Renderer::globalIndicesBuffer.GetBufferHandle());

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData = { vertexBufferAddress + Renderer::globalVertexBuffer.GetMemoryOffset(mesh.vertexMemory) };
	triangles.vertexStride = sizeof(Vertex);
	triangles.maxVertex = mesh.vertices.size();
	triangles.indexType = VK_INDEX_TYPE_UINT16;
	triangles.indexData = { indexBufferAddress + Renderer::globalIndicesBuffer.GetMemoryOffset(mesh.indexMemory) };
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

	std::vector<uint32_t> BLMaxPrimitiveCounts = { (uint32_t)mesh.faceCount };
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

	Vulkan::YieldCommandPool(creationObject.queueIndex, commandPool);

	BLAS->Build(creationObject, &BLASGeometry, BLASBuildSizesInfo, mesh.faceCount);

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
	BLASBuildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	BLASBuildGeometryInfo.geometryCount = 1;
	BLASBuildGeometryInfo.pGeometries = pGeometries;
	BLASBuildGeometryInfo.scratchData = { 0 };

	VkAccelerationStructureDeviceAddressInfoKHR BLASAddressInfo{};
	BLASAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	BLASAddressInfo.accelerationStructure = accelerationStructure;

	deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(creationObject.logicalDevice, &BLASAddressInfo);

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, buildSizesInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchDeviceMemory);

	VkDeviceAddress scratchDeviceAddress = Vulkan::GetDeviceAddress(creationObject.logicalDevice, scratchBuffer);

	BLASBuildGeometryInfo.scratchData = { scratchDeviceAddress };
	BLASBuildGeometryInfo.dstAccelerationStructure = accelerationStructure;

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

TopLevelAccelerationStructure* TopLevelAccelerationStructure::CreateTopLevelAccelerationStructure(const VulkanCreationObject& creationObject, std::vector<BottomLevelAccelerationStructure*> BLAS)
{
	if (!TLASInstancesIsInit)
	{
		instanceBuffer.Reserve(creationObject, 500, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		TLASInstancesIsInit = true;
	}
	instanceBuffer.Clear(creationObject);

	TopLevelAccelerationStructure* TLAS = new TopLevelAccelerationStructure();
	TLAS->logicalDevice = creationObject.logicalDevice;

	VkCommandPool commandPool = Vulkan::FetchNewCommandPool(creationObject);

	std::vector<VkAccelerationStructureInstanceKHR> BLASInstances(BLAS.size());
	for (int i = 0; i < BLAS.size(); i++)
	{
		VkAccelerationStructureInstanceKHR BLASInstance{};
		BLASInstance.transform = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 }; // instead of this use std::memcpy to copy the glm::mat4 transform data to this transform
		BLASInstance.instanceCustomIndex = i;
		BLASInstance.mask = 0xFF;
		BLASInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		BLASInstance.accelerationStructureReference = BLAS[i]->deviceAddress;

		BLASInstances.push_back(BLASInstance);
	}
	instanceBuffer.SubmitNewData(BLASInstances); // make it so that only the new BLASs get submitted instead of all of the BLASs (even the old ones). the code right now is a REALLY bad implementation

	VkAccelerationStructureGeometryInstancesDataKHR instances{};
	instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instances.arrayOfPointers = VK_FALSE;
	instances.data = { Vulkan::GetDeviceAddress(creationObject.logicalDevice, instanceBuffer.GetBufferHandle()) };

	VkAccelerationStructureGeometryDataKHR TLASGeometryData{};
	TLASGeometryData.instances = instances;
	
	VkAccelerationStructureGeometryKHR TLASGeometry{};
	TLASGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	TLASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	TLASGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	TLASGeometry.geometry = TLASGeometryData;
	const VkAccelerationStructureGeometryKHR* pGeometry = &TLASGeometry;

	VkAccelerationStructureBuildSizesInfoKHR TLASBuildSizesInfo{};
	TLASBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	VkAccelerationStructureBuildGeometryInfoKHR TLASBuildGeometryInfo{};
	TLASBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	TLASBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	TLASBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	TLASBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	TLASBuildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	TLASBuildGeometryInfo.geometryCount = 1; // must be 1 according to the spec
	TLASBuildGeometryInfo.pGeometries = pGeometry;
	TLASBuildGeometryInfo.scratchData = { 0 };

	vkGetAccelerationStructureBuildSizesKHR(creationObject.logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &TLASBuildGeometryInfo, &MAX_PRIMITIVES, &TLASBuildSizesInfo);

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, TLASBuildSizesInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TLAS->buffer, TLAS->deviceMemory);

	VkAccelerationStructureCreateInfoKHR TLASCreateInfo{};
	TLASCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	TLASCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	TLASCreateInfo.size = TLASBuildSizesInfo.accelerationStructureSize;
	TLASCreateInfo.buffer = TLAS->buffer;
	TLASCreateInfo.deviceAddress = 0;

	VkResult result = vkCreateAccelerationStructureKHR(creationObject.logicalDevice, &TLASCreateInfo, nullptr, &TLAS->accelerationStructure);
	CheckVulkanResult("Failed to create the TLAS", result, nameof(vkCreateAccelerationStructureKHR));
	
	Vulkan::YieldCommandPool(creationObject.queueIndex, commandPool);

	TLAS->Build(creationObject, pGeometry, TLASBuildSizesInfo, BLASInstances.size());

	return TLAS;
}

void TopLevelAccelerationStructure::Build(const VulkanCreationObject& creationObject, const VkAccelerationStructureGeometryKHR* pGeometry, const VkAccelerationStructureBuildSizesInfoKHR& buildSizesInfo, uint32_t instanceCount)
{
	VkCommandPool commandPool = Vulkan::FetchNewCommandPool(creationObject);

	VkAccelerationStructureBuildGeometryInfoKHR TLASBuildGeometryInfo{};
	TLASBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	TLASBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	TLASBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	TLASBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	TLASBuildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	TLASBuildGeometryInfo.geometryCount = 1; // must be 1 according to the spec
	TLASBuildGeometryInfo.pGeometries = pGeometry;
	TLASBuildGeometryInfo.scratchData = { 0 };

	VkAccelerationStructureDeviceAddressInfoKHR TLASAddressInfo{};
	TLASAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	TLASAddressInfo.accelerationStructure = accelerationStructure;

	VkDeviceAddress TLASDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(creationObject.logicalDevice, &TLASAddressInfo);

	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, buildSizesInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

	VkDeviceAddress TLASScratchBufferAddress = Vulkan::GetDeviceAddress(creationObject.logicalDevice, scratchBuffer);

	TLASBuildGeometryInfo.dstAccelerationStructure = accelerationStructure;
	TLASBuildGeometryInfo.scratchData = { TLASScratchBufferAddress };

	VkAccelerationStructureBuildRangeInfoKHR TLASBuildRangeInfo{};
	TLASBuildRangeInfo.primitiveCount = instanceCount;
	const VkAccelerationStructureBuildRangeInfoKHR* pTLASBuildRangeInfos = &TLASBuildRangeInfo;

	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(creationObject.logicalDevice, commandPool);
	vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &TLASBuildGeometryInfo, &pTLASBuildRangeInfos);
	Vulkan::EndSingleTimeCommands(creationObject.logicalDevice, creationObject.queue, commandBuffer, commandPool);

	Vulkan::YieldCommandPool(creationObject.queueIndex, commandPool);
}

void TopLevelAccelerationStructure::Destroy()
{
	vkFreeMemory(logicalDevice, scratchMemory, nullptr);
	vkDestroyBuffer(logicalDevice, scratchBuffer, nullptr);

	vkDestroyAccelerationStructureKHR(logicalDevice, accelerationStructure, nullptr);

	vkFreeMemory(logicalDevice, deviceMemory, nullptr);
	vkDestroyBuffer(logicalDevice, buffer, nullptr);
}