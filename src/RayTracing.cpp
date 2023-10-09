#include <fstream>
#include "renderer/RayTracing.h"
#include "renderer/Vulkan.h"
#include "SceneLoader.h"
#include "Vertex.h"

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 1;

VkCommandPool commandPool;
std::vector<VkCommandBuffer> commandBuffers(MAX_FRAMES_IN_FLIGHT);
VkDescriptorPool descriptorPool;
VkDescriptorSetLayout descriptorSetLayout;
VkDescriptorSetLayout materialSetLayout;
std::vector<VkDescriptorSet> descriptorSets;

VkQueue queue;

VkPipelineLayout pipelineLayout;
VkPipeline pipeline;

VkBuffer BLASBuffer; // todo: seperate BLAS into indepedent class
VkDeviceMemory BLASDeviceMemory;
VkAccelerationStructureKHR BLAS;

VkBuffer BLASScratchBuffer;
VkDeviceMemory BLASSscratchDeviceMemory;

std::vector<char> ReadShaderFile(const std::string& filePath)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);
	if (!file.is_open())
		throw std::runtime_error("Failed to open the shader at " + filePath);

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();
	return buffer;
}

PFN_vkGetBufferDeviceAddressKHR pvkGetBufferDeviceAddressKHR;
PFN_vkCreateRayTracingPipelinesKHR pvkCreateRayTracingPipelinesKHR;
PFN_vkGetAccelerationStructureBuildSizesKHR pvkGetAccelerationStructureBuildSizesKHR;
PFN_vkCreateAccelerationStructureKHR pvkCreateAccelerationStructureKHR;
PFN_vkDestroyAccelerationStructureKHR pvkDestroyAccelerationStructureKHR;
PFN_vkGetAccelerationStructureDeviceAddressKHR pvkGetAccelerationStructureDeviceAddressKHR;
PFN_vkCmdBuildAccelerationStructuresKHR pvkCmdBuildAccelerationStructuresKHR;
PFN_vkGetRayTracingShaderGroupHandlesKHR pvkGetRayTracingShaderGroupHandlesKHR;
PFN_vkCmdTraceRaysKHR pvkCmdTraceRaysKHR;

void RayTracing::Destroy(VkDevice logicalDevice)
{
	pvkDestroyAccelerationStructureKHR(logicalDevice, BLAS, nullptr);

	vkDestroyBuffer(logicalDevice, BLASScratchBuffer, nullptr);
	vkFreeMemory(logicalDevice, BLASSscratchDeviceMemory, nullptr);

	vkDestroyBuffer(logicalDevice, BLASBuffer, nullptr);
	vkFreeMemory(logicalDevice, BLASDeviceMemory, nullptr);

	vkDestroyPipeline(logicalDevice, pipeline, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);

	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, materialSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);

	vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
}

void FetchRayTracingFunctions(VkDevice logicalDevice)
{
	pvkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(logicalDevice, "vkGetBufferDeviceAddressKHR");
	pvkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(logicalDevice, "vkCreateRayTracingPipelinesKHR");
	pvkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(logicalDevice, "vkGetAccelerationStructureBuildSizesKHR");
	pvkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(logicalDevice, "vkCreateAccelerationStructureKHR");
	pvkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(logicalDevice, "vkGetAccelerationStructureDeviceAddressKHR");
	pvkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(logicalDevice, "vkCmdBuildAccelerationStructuresKHR");
	pvkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(logicalDevice, "vkDestroyAccelerationStructureKHR");
}

void RayTracing::Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Object* object)
{
	FetchRayTracingFunctions(logicalDevice);

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties{};
	rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

	VkPhysicalDeviceProperties2 properties2{};
	properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &rayTracingProperties;

	vkGetPhysicalDeviceProperties2(physicalDevice.Device(), &properties2);

	if (!physicalDevice.QueueFamilies(surface).graphicsFamily.has_value())
		throw std::runtime_error("No appropriate graphics family could be found for ray tracing");

	uint32_t queueFamilyIndex = physicalDevice.QueueFamilies(surface).graphicsFamily.value();
	vkGetDeviceQueue(logicalDevice, queueFamilyIndex, 0, &queue);

	// command pool

	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

	VkResult result = vkCreateCommandPool(logicalDevice, &commandPoolCreateInfo, nullptr, &commandPool);
	if (vkCreateCommandPool(logicalDevice, &commandPoolCreateInfo, nullptr, &commandPool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create a command pool for ray tracing");

	// command buffer

	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

	if (vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, commandBuffers.data()) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate the command buffers for ray tracing");

	// descriptor pool (frames in flight not implemented)

	std::vector<VkDescriptorPoolSize> descriptorPoolSizes(4);
	descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	descriptorPoolSizes[0].descriptorCount = 1;
	descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSizes[1].descriptorCount = 1;
	descriptorPoolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorPoolSizes[2].descriptorCount = 4;
	descriptorPoolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorPoolSizes[3].descriptorCount = 1;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	descriptorPoolCreateInfo.maxSets = 2;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

	if (vkCreateDescriptorPool(logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create the descriptor pool for ray tracing");

	// descriptor set layout (frames in flight not implemented)

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings(5);

	setLayoutBindings[0].binding = 0;
	setLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	setLayoutBindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	setLayoutBindings[0].descriptorCount = 1;
	setLayoutBindings[0].pImmutableSamplers = nullptr;

	setLayoutBindings[1].binding = 1;
	setLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	setLayoutBindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	setLayoutBindings[1].descriptorCount = 1;
	setLayoutBindings[1].pImmutableSamplers = nullptr;

	setLayoutBindings[2].binding = 2;
	setLayoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	setLayoutBindings[2].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	setLayoutBindings[2].descriptorCount = 1;
	setLayoutBindings[2].pImmutableSamplers = nullptr;

	setLayoutBindings[3].binding = 3;
	setLayoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	setLayoutBindings[3].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	setLayoutBindings[3].descriptorCount = 1;
	setLayoutBindings[3].pImmutableSamplers = nullptr;

	setLayoutBindings[4].binding = 4;
	setLayoutBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	setLayoutBindings[4].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	setLayoutBindings[4].descriptorCount = 1;
	setLayoutBindings[4].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	layoutCreateInfo.pBindings = setLayoutBindings.data();

	if (vkCreateDescriptorSetLayout(logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create the descriptor set layout for ray tracing");

	// material set layouts (placeholder material, not Material.h materials) (frames in flight not implemented)

	std::vector<VkDescriptorSetLayoutBinding> materialLayoutBindings(2);

	materialLayoutBindings[0].binding = 0;
	materialLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialLayoutBindings[0].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	materialLayoutBindings[0].descriptorCount = 1;
	materialLayoutBindings[0].pImmutableSamplers = nullptr;

	materialLayoutBindings[1].binding = 1;
	materialLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialLayoutBindings[1].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	materialLayoutBindings[1].descriptorCount = 1;
	materialLayoutBindings[1].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo materialLayoutCreateInfo{};
	materialLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	materialLayoutCreateInfo.bindingCount = static_cast<uint32_t>(materialLayoutBindings.size());
	materialLayoutCreateInfo.pBindings = materialLayoutBindings.data();

	if (vkCreateDescriptorSetLayout(logicalDevice, &layoutCreateInfo, nullptr, &materialSetLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create the material descriptor set layout for ray tracing");

	// allocate the descriptor sets

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts{ descriptorSetLayout, materialSetLayout };

	VkDescriptorSetAllocateInfo setAllocateInfo{};
	setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocateInfo.descriptorPool = descriptorPool;
	setAllocateInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	setAllocateInfo.pSetLayouts = descriptorSetLayouts.data();

	descriptorSets.resize(descriptorSetLayouts.size());
	if (vkAllocateDescriptorSets(logicalDevice, &setAllocateInfo, descriptorSets.data()) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate the descriptor sets for ray tracing");

	// pipeline layout

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create the pipeline layout for ray tracing");

	// shaders (raygen rayhit raymiss)

	std::vector<char> genShaderSource = ReadShaderFile("shaders/gen.rgen.spv");
	VkShaderModule genShader = Vulkan::CreateShaderModule(logicalDevice, genShaderSource);

	std::vector<char> chitShaderSource = ReadShaderFile("shaders/hit.rchit.spv");
	VkShaderModule hitShader = Vulkan::CreateShaderModule(logicalDevice, chitShaderSource);

	std::vector<char> missShaderSource = ReadShaderFile("shaders/miss.rmiss.spv");
	VkShaderModule missShader = Vulkan::CreateShaderModule(logicalDevice, missShaderSource);

	std::vector<char> shadowShaderSource = ReadShaderFile("shaders/shadow.rmiss.spv");
	VkShaderModule shadowShader = Vulkan::CreateShaderModule(logicalDevice, shadowShaderSource);

	// pipeline

	std::vector<VkPipelineShaderStageCreateInfo> pipelineStageCreateInfos(4);
	pipelineStageCreateInfos[0] = Vulkan::GetGenericShaderStageCreateInfo(hitShader, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
	pipelineStageCreateInfos[1] = Vulkan::GetGenericShaderStageCreateInfo(genShader, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	pipelineStageCreateInfos[2] = Vulkan::GetGenericShaderStageCreateInfo(missShader, VK_SHADER_STAGE_MISS_BIT_KHR);
	pipelineStageCreateInfos[3] = Vulkan::GetGenericShaderStageCreateInfo(shadowShader, VK_SHADER_STAGE_MISS_BIT_KHR);

	std::vector<VkRayTracingShaderGroupCreateInfoKHR> RTShaderGroupCreateInfos(4);
	RTShaderGroupCreateInfos[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	RTShaderGroupCreateInfos[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	RTShaderGroupCreateInfos[0].generalShader = VK_SHADER_UNUSED_KHR;
	RTShaderGroupCreateInfos[0].anyHitShader = VK_SHADER_UNUSED_KHR;
	RTShaderGroupCreateInfos[0].intersectionShader = VK_SHADER_UNUSED_KHR;
	RTShaderGroupCreateInfos[0].closestHitShader = 0;

	for (uint32_t i = 1; i < RTShaderGroupCreateInfos.size(); i++)
	{
		RTShaderGroupCreateInfos[i].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		RTShaderGroupCreateInfos[i].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		RTShaderGroupCreateInfos[i].closestHitShader = VK_SHADER_UNUSED_KHR;
		RTShaderGroupCreateInfos[i].anyHitShader = VK_SHADER_UNUSED_KHR;
		RTShaderGroupCreateInfos[i].intersectionShader = VK_SHADER_UNUSED_KHR;
		RTShaderGroupCreateInfos[i].generalShader = i;
	}

	VkRayTracingPipelineCreateInfoKHR RTPipelineCreateInfo{};
	RTPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	RTPipelineCreateInfo.stageCount = static_cast<uint32_t>(pipelineStageCreateInfos.size());
	RTPipelineCreateInfo.pStages = pipelineStageCreateInfos.data();
	RTPipelineCreateInfo.groupCount = static_cast<uint32_t>(RTShaderGroupCreateInfos.size());
	RTPipelineCreateInfo.pGroups = RTShaderGroupCreateInfos.data();
	RTPipelineCreateInfo.maxPipelineRayRecursionDepth = 1;
	RTPipelineCreateInfo.layout = pipelineLayout;
	RTPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	RTPipelineCreateInfo.basePipelineIndex = 0;

	if (pvkCreateRayTracingPipelinesKHR(logicalDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &RTPipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS)
		throw std::runtime_error("Failed to create the pipeline for ray tracing");

	vkDestroyShaderModule(logicalDevice, shadowShader, nullptr);
	vkDestroyShaderModule(logicalDevice, missShader, nullptr);
	vkDestroyShaderModule(logicalDevice, hitShader, nullptr);
	vkDestroyShaderModule(logicalDevice, genShader, nullptr);

	// bottom level acceleration structure
	VkBufferDeviceAddressInfo bufferAddressInfo{};
	bufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferAddressInfo.buffer = object->meshes[0].vertexBuffer.GetVkBuffer();

	VkDeviceAddress vertexBufferAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &bufferAddressInfo);

	bufferAddressInfo.buffer = object->meshes[0].indexBuffer.GetVkBuffer();
	
	VkDeviceAddress indexBufferAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &bufferAddressInfo);

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData = { vertexBufferAddress };
	triangles.vertexStride = sizeof(Vertex);
	triangles.maxVertex = static_cast<uint32_t>(object->meshes[0].vertices.size());
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
	BLASBuildSizesInfo.accelerationStructureSize = 0;
	BLASBuildSizesInfo.updateScratchSize = 0;
	BLASBuildSizesInfo.buildScratchSize = 0;

	std::vector<uint32_t> BLMaxPrimitiveCounts = { static_cast<uint32_t>(object->meshes[0].vertices.size()) };
	pvkGetAccelerationStructureBuildSizesKHR(logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &BLASBuildGeometryInfo, BLMaxPrimitiveCounts.data(), &BLASBuildSizesInfo);

	VkBufferCreateInfo BLASBufferCreateInfo{};
	BLASBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	BLASBufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
	BLASBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	BLASBufferCreateInfo.size = BLASBuildSizesInfo.accelerationStructureSize;
	BLASBufferCreateInfo.queueFamilyIndexCount = 1;
	BLASBufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;

	if (vkCreateBuffer(logicalDevice, &BLASBufferCreateInfo, nullptr, &BLASBuffer) != VK_SUCCESS)
		throw std::runtime_error("Failed to create the BLAS buffer for ray tracing");

	VkMemoryRequirements BLASMemRequirements;
	vkGetBufferMemoryRequirements(logicalDevice, BLASBuffer, &BLASMemRequirements);

	uint32_t BLASMemoryTypeIndex = Vulkan::GetMemoryType(BLASMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physicalDevice);

	VkMemoryAllocateInfo BLASMemAllocateInfo{};
	BLASMemAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	BLASMemAllocateInfo.allocationSize = BLASMemRequirements.size;
	BLASMemAllocateInfo.memoryTypeIndex = BLASMemoryTypeIndex;

	if (vkAllocateMemory(logicalDevice, &BLASMemAllocateInfo, nullptr, &BLASDeviceMemory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate the memory for a BLAS");

	if (vkBindBufferMemory(logicalDevice, BLASBuffer, BLASDeviceMemory, 0) != VK_SUCCESS)
		throw std::runtime_error("Failed to bind the BLAS buffer memory");

	VkAccelerationStructureCreateInfoKHR BLASCreateInfo{};
	BLASCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	BLASCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	BLASCreateInfo.size = BLASBuildSizesInfo.accelerationStructureSize;
	BLASCreateInfo.buffer = BLASBuffer;
	BLASCreateInfo.offset = 0;

	if (pvkCreateAccelerationStructureKHR(logicalDevice, &BLASCreateInfo, nullptr, &BLAS) != VK_SUCCESS)
		throw std::runtime_error("Failed to create the BLAS");

	// build BLAS (presumably for a frame)

	VkAccelerationStructureDeviceAddressInfoKHR BLASAddressInfo{};
	BLASAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	BLASAddressInfo.accelerationStructure = BLAS;

	VkDeviceAddress BLASDeviceAddress = pvkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &BLASAddressInfo);

	VkBufferCreateInfo BLASScratchBufferCreateInfo{};
	BLASScratchBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	BLASScratchBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	BLASScratchBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	BLASScratchBufferCreateInfo.size = BLASBuildSizesInfo.buildScratchSize;
	BLASScratchBufferCreateInfo.queueFamilyIndexCount = 1;
	BLASScratchBufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;

	if (vkCreateBuffer(logicalDevice, &BLASScratchBufferCreateInfo, nullptr, &BLASScratchBuffer) != VK_SUCCESS)
		throw std::runtime_error("Failed to create the scratch buffer for a BLAS");

	VkMemoryRequirements BLASScratchMemRequirements;
	vkGetBufferMemoryRequirements(logicalDevice, BLASScratchBuffer, &BLASScratchMemRequirements);
	
	uint32_t BLASScratchMemTypeIndex = Vulkan::GetMemoryType(BLASScratchMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physicalDevice);

	VkMemoryAllocateInfo BLASScratchAllocateInfo{};
	BLASScratchAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	BLASScratchAllocateInfo.pNext = Vulkan::optionalMemoryAllocationFlags;
	BLASScratchAllocateInfo.allocationSize = BLASScratchMemRequirements.size;
	BLASScratchAllocateInfo.memoryTypeIndex = BLASScratchMemTypeIndex;

	if (vkAllocateMemory(logicalDevice, &BLASScratchAllocateInfo, nullptr, &BLASSscratchDeviceMemory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate the memory for the BLAS scratch buffer");

	if (vkBindBufferMemory(logicalDevice, BLASScratchBuffer, BLASSscratchDeviceMemory, 0) != VK_SUCCESS)
		throw std::runtime_error("Failed to bind the scratch buffer");

	VkBufferDeviceAddressInfo scratchDeviceAddressInfo{};
	scratchDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	scratchDeviceAddressInfo.buffer = BLASScratchBuffer;

	VkDeviceAddress scratchDeviceAddress = vkGetBufferDeviceAddress(logicalDevice, &scratchDeviceAddressInfo);

	BLASBuildGeometryInfo.scratchData = { scratchDeviceAddress };
	BLASBuildGeometryInfo.dstAccelerationStructure = BLAS;

	VkAccelerationStructureBuildRangeInfoKHR BLASBuildRangeInfo{};
	BLASBuildRangeInfo.primitiveCount = object->meshes[0].vertices.size();
	BLASBuildRangeInfo.primitiveOffset = 0;
	BLASBuildRangeInfo.firstVertex = 0;
	BLASBuildRangeInfo.transformOffset = 0;
	const VkAccelerationStructureBuildRangeInfoKHR* pBLASBuildRangeInfo = &BLASBuildRangeInfo;

	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);
	pvkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &BLASBuildGeometryInfo, &pBLASBuildRangeInfo);
	Vulkan::EndSingleTimeCommands(logicalDevice, queue, commandBuffer, commandPool);
	
	Destroy(logicalDevice);
}