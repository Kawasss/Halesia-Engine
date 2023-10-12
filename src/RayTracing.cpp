#include <fstream>
#include <thread>
#include "renderer/RayTracing.h"
#include "renderer/Vulkan.h"
#include "SceneLoader.h"
#include "Vertex.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#define nameof(s) #s
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 1;

std::vector<VkCommandBuffer> commandBuffers(MAX_FRAMES_IN_FLIGHT);
VkDescriptorPool descriptorPool;
VkDescriptorSetLayout descriptorSetLayout;
VkDescriptorSetLayout materialSetLayout;
std::vector<VkDescriptorSet> descriptorSets;

VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties{};

VulkanCreationObject creationObject;
uint32_t queueFamilyIndex;
VkQueue queue;

VkPipelineLayout pipelineLayout;
VkPipeline pipeline;

VkBuffer BLASBuffer; // todo: seperate BLAS into indepedent class
VkDeviceMemory BLASDeviceMemory;
VkAccelerationStructureKHR BLAS;

VkBuffer BLASScratchBuffer;
VkDeviceMemory BLASSscratchDeviceMemory;

VkBuffer BLGeometryInstanceBuffer;
VkDeviceMemory BLGeometryInstanceBufferMemory;

VkBuffer TLASBuffer;
VkDeviceMemory TLASBufferMemory;
VkAccelerationStructureKHR TLAS;

VkBuffer TLASScratchBuffer;
VkDeviceMemory TLASScratchMemory;

VkBuffer uniformBufferBuffer;
VkDeviceMemory uniformBufferMemory;

uint32_t verticesSize;
uint32_t indicesSize;
VulkanBuffer testObjectVertexBuffer;
IndexBuffer testObjectIndexBuffer;

struct UniformBuffer
{
	glm::vec3 position;
	glm::vec3 right;
	glm::vec3 up;
	glm::vec3 forward;

	uint32_t frameCount;
};

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

void CreateTestObject(BufferCreationObject creationObject)
{
	const aiScene* scene = aiImportFile("stdObj/monkey.obj", aiProcessPreset_TargetRealtime_Fast);
	if (scene == nullptr)
		throw std::runtime_error("Failed to read the test model");

	std::vector<glm::vec3> vertices;
	std::vector<uint16_t> indices;

	for (int i = 0; i < scene->mMeshes[0]->mNumVertices; i++)
		vertices.push_back({ scene->mMeshes[0]->mVertices[i].x, scene->mMeshes[0]->mVertices[i].y, scene->mMeshes[0]->mVertices[i].z });

	for (int i = 0; i < scene->mMeshes[0]->mNumFaces; i++)
		for (int j = 0; j < scene->mMeshes[0]->mFaces[i].mNumIndices; j++)
			indices.push_back(scene->mMeshes[0]->mFaces[i].mIndices[j]);

	testObjectVertexBuffer = VulkanBuffer(creationObject, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vertices);
	testObjectIndexBuffer = IndexBuffer(creationObject, indices);
	verticesSize = vertices.size();
	indicesSize = indices.size();
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
	testObjectIndexBuffer.Destroy();
	testObjectVertexBuffer.Destroy();

	vkFreeMemory(logicalDevice, uniformBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, uniformBufferBuffer, nullptr);

	vkFreeMemory(logicalDevice, TLASScratchMemory, nullptr);
	vkDestroyBuffer(logicalDevice, TLASScratchBuffer, nullptr);

	pvkDestroyAccelerationStructureKHR(logicalDevice, TLAS, nullptr);

	vkFreeMemory(logicalDevice, TLASBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, TLASBuffer, nullptr);

	vkFreeMemory(logicalDevice, BLGeometryInstanceBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, BLGeometryInstanceBuffer, nullptr);

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
	pvkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(logicalDevice, "vkGetRayTracingShaderGroupHandlesKHR");
	pvkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(logicalDevice, "vkCmdTraceRaysKHR");
}

void RayTracing::Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Object* object, Camera* camera)
{
	VkResult result = VK_SUCCESS;

	this->logicalDevice = logicalDevice;
	this->physicalDevice = physicalDevice;

	FetchRayTracingFunctions(logicalDevice);

	rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

	VkPhysicalDeviceProperties2 properties2{};
	properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &rayTracingProperties;

	vkGetPhysicalDeviceProperties2(physicalDevice.Device(), &properties2);

	if (!physicalDevice.QueueFamilies(surface).graphicsFamily.has_value())
		throw VulkanAPIError("No appropriate graphics family could be found for ray tracing", VK_SUCCESS, nameof(physicalDevice.QueueFamilies(surface).graphicsFamily.has_value()), __FILENAME__, std::to_string(__LINE__));

	queueFamilyIndex = physicalDevice.QueueFamilies(surface).graphicsFamily.value();
	vkGetDeviceQueue(logicalDevice, queueFamilyIndex, 0, &queue);

	// command pool

	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

	result = vkCreateCommandPool(logicalDevice, &commandPoolCreateInfo, nullptr, &commandPool);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create a command pool for ray tracing", result, nameof(vkCreateCommandPool), __FILENAME__, std::to_string(__LINE__));

	// creation object

	creationObject = { logicalDevice, physicalDevice, commandPool, queue };
	CreateTestObject(creationObject);
	Vulkan::globalThreadingMutex->lock();

	// command buffer

	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

	result = vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, commandBuffers.data());
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to allocate the command buffers for ray tracing", result, nameof(vkAllocateCommandBuffers), __FILENAME__, std::to_string(__LINE__));

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

	result = vkCreateDescriptorPool(logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the descriptor pool for ray tracing", result, nameof(vkCreateDescriptorPool), __FILENAME__, std::to_string(__LINE__));

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

	result = vkCreateDescriptorSetLayout(logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the descriptor set layout for ray tracing", result, nameof(vkCreateDescriptorSetLayout), __FILENAME__, std::to_string(__LINE__));

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

	result = vkCreateDescriptorSetLayout(logicalDevice, &materialLayoutCreateInfo, nullptr, &materialSetLayout);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the material descriptor set layout for ray tracing", result, nameof(vkCreateDescriptorSetLayout), __FILENAME__, std::to_string(__LINE__));

	// allocate the descriptor sets

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts{ descriptorSetLayout, materialSetLayout };

	VkDescriptorSetAllocateInfo setAllocateInfo{};
	setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocateInfo.descriptorPool = descriptorPool;
	setAllocateInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	setAllocateInfo.pSetLayouts = descriptorSetLayouts.data();

	descriptorSets.resize(descriptorSetLayouts.size());

	result = vkAllocateDescriptorSets(logicalDevice, &setAllocateInfo, descriptorSets.data());
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to allocate the descriptor sets for ray tracing", result, nameof(vkAllocateDescriptorSets), __FILENAME__, std::to_string(__LINE__));

	// pipeline layout

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	result = vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the pipeline layout for ray tracing", result, nameof(vkCreatePipelineLayout), __FILENAME__, std::to_string(__LINE__));

	// shaders

	VkShaderModule genShader = Vulkan::CreateShaderModule(logicalDevice, ReadShaderFile("shaders/gen.rgen.spv"));
	VkShaderModule hitShader = Vulkan::CreateShaderModule(logicalDevice, ReadShaderFile("shaders/hit.rchit.spv"));
	VkShaderModule missShader = Vulkan::CreateShaderModule(logicalDevice, ReadShaderFile("shaders/miss.rmiss.spv"));
	VkShaderModule shadowShader = Vulkan::CreateShaderModule(logicalDevice, ReadShaderFile("shaders/shadow.rmiss.spv"));

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

	result = pvkCreateRayTracingPipelinesKHR(logicalDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &RTPipelineCreateInfo, nullptr, &pipeline);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the pipeline for ray tracing", result, nameof(pvkCreateRayTracingPipelinesKHR), __FILENAME__, std::to_string(__LINE__));

	vkDestroyShaderModule(logicalDevice, shadowShader, nullptr);
	vkDestroyShaderModule(logicalDevice, missShader, nullptr);
	vkDestroyShaderModule(logicalDevice, hitShader, nullptr);
	vkDestroyShaderModule(logicalDevice, genShader, nullptr);

	// bottom level acceleration structure

	VkBufferDeviceAddressInfo bufferAddressInfo{};
	bufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferAddressInfo.buffer = testObjectVertexBuffer.GetVkBuffer();

	VkDeviceAddress vertexBufferAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &bufferAddressInfo);

	bufferAddressInfo.buffer = testObjectIndexBuffer.GetVkBuffer();
	
	VkDeviceAddress indexBufferAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &bufferAddressInfo);

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData = { vertexBufferAddress };
	triangles.vertexStride = sizeof(Vertex);
	triangles.maxVertex = verticesSize;//static_cast<uint32_t>(object->meshes[0].vertices.size());
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

	std::vector<uint32_t> BLMaxPrimitiveCounts = { verticesSize/*static_cast<uint32_t>(object->meshes[0].vertices.size())*/ };
	pvkGetAccelerationStructureBuildSizesKHR(logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &BLASBuildGeometryInfo, BLMaxPrimitiveCounts.data(), &BLASBuildSizesInfo);

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, BLASBuildSizesInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, BLASBuffer, BLASDeviceMemory);

	VkAccelerationStructureCreateInfoKHR BLASCreateInfo{};
	BLASCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	BLASCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	BLASCreateInfo.size = BLASBuildSizesInfo.accelerationStructureSize;
	BLASCreateInfo.buffer = BLASBuffer;
	BLASCreateInfo.offset = 0;

	result = pvkCreateAccelerationStructureKHR(logicalDevice, &BLASCreateInfo, nullptr, &BLAS);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the BLAS", result, nameof(pvkCreateAccelerationStructureKHR), __FILENAME__, std::to_string(__LINE__));

	// build BLAS

	VkAccelerationStructureDeviceAddressInfoKHR BLASAddressInfo{};
	BLASAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	BLASAddressInfo.accelerationStructure = BLAS;

	VkDeviceAddress BLASDeviceAddress = pvkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &BLASAddressInfo);

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, BLASBuildSizesInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, BLASScratchBuffer, BLASSscratchDeviceMemory);

	VkBufferDeviceAddressInfo scratchDeviceAddressInfo{};
	scratchDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	scratchDeviceAddressInfo.buffer = BLASScratchBuffer;

	VkDeviceAddress scratchDeviceAddress = vkGetBufferDeviceAddress(logicalDevice, &scratchDeviceAddressInfo);

	BLASBuildGeometryInfo.scratchData = { scratchDeviceAddress };
	BLASBuildGeometryInfo.dstAccelerationStructure = BLAS;

	VkAccelerationStructureBuildRangeInfoKHR BLASBuildRangeInfo{};
	BLASBuildRangeInfo.primitiveCount = verticesSize / 3;// object->meshes[0].vertices.size();
	const VkAccelerationStructureBuildRangeInfoKHR* pBLASBuildRangeInfo = &BLASBuildRangeInfo;

	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);
	pvkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &BLASBuildGeometryInfo, &pBLASBuildRangeInfo);
	Vulkan::EndSingleTimeCommands(logicalDevice, queue, commandBuffer, commandPool);
	
	// TLAS

	VkAccelerationStructureInstanceKHR BLASInstance{};
	BLASInstance.transform = {  1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
	BLASInstance.instanceCustomIndex = 0;
	BLASInstance.mask = 0xFF;
	BLASInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	BLASInstance.accelerationStructureReference = BLASDeviceAddress;

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, sizeof(VkAccelerationStructureInstanceKHR), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, BLGeometryInstanceBuffer, BLGeometryInstanceBufferMemory);
	
	void* BLGeometryInstanceBufferMemPtr;

	result = vkMapMemory(logicalDevice, BLGeometryInstanceBufferMemory, 0, sizeof(VkAccelerationStructureInstanceKHR), 0, &BLGeometryInstanceBufferMemPtr);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to map the memory of the bottom level geometry instance buffer", result, nameof(vkMapMemory), __FILENAME__, std::to_string(__LINE__));

	memcpy(BLGeometryInstanceBufferMemPtr, &BLASInstance, sizeof(VkAccelerationStructureInstanceKHR));
	vkUnmapMemory(logicalDevice, BLGeometryInstanceBufferMemory);

	VkBufferDeviceAddressInfo BLGeometryInstanceAddressInfo{};
	BLGeometryInstanceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	BLGeometryInstanceAddressInfo.buffer = BLGeometryInstanceBuffer;

	VkDeviceAddress BLGeometryInstanceAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &BLGeometryInstanceAddressInfo);

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

	std::vector<uint32_t> TLASMaxPrimitiveCounts = { 1 };

	pvkGetAccelerationStructureBuildSizesKHR(logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &TLASBuildGeometryInfo, TLASMaxPrimitiveCounts.data(), &TLASBuildSizesInfo);

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, TLASBuildSizesInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TLASBuffer, TLASBufferMemory);

	VkAccelerationStructureCreateInfoKHR TLASCreateInfo{};
	TLASCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	TLASCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	TLASCreateInfo.size = TLASBuildSizesInfo.accelerationStructureSize;
	TLASCreateInfo.buffer = TLASBuffer;
	TLASCreateInfo.deviceAddress = 0;

	result = pvkCreateAccelerationStructureKHR(logicalDevice, &TLASCreateInfo, nullptr, &TLAS);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the TLAS", result, nameof(pvkCreateAccelerationStructureKHR), __FILENAME__, std::to_string(__LINE__));

	// build TLAS

	VkAccelerationStructureDeviceAddressInfoKHR TLASAddressInfo{};
	TLASAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	TLASAddressInfo.accelerationStructure = TLAS;

	VkDeviceAddress TLASDeviceAddress = pvkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &TLASAddressInfo);

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, TLASBuildSizesInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TLASScratchBuffer, TLASScratchMemory);

	VkBufferDeviceAddressInfo TLASSratchBufferAddressInfo{};
	TLASSratchBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	TLASSratchBufferAddressInfo.buffer = TLASScratchBuffer;

	VkDeviceAddress TLASScratchBufferAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &TLASSratchBufferAddressInfo);

	TLASBuildGeometryInfo.dstAccelerationStructure = TLAS;
	TLASBuildGeometryInfo.scratchData = { TLASScratchBufferAddress };

	VkAccelerationStructureBuildRangeInfoKHR TLASBuildRangeInfo{};
	TLASBuildRangeInfo.primitiveCount = 1;
	const VkAccelerationStructureBuildRangeInfoKHR* pTLASBuildRangeInfos = &TLASBuildRangeInfo;
	
	commandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);
	pvkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &TLASBuildGeometryInfo, &pTLASBuildRangeInfos);
	Vulkan::EndSingleTimeCommands(logicalDevice, queue, commandBuffer, commandPool);

	// uniform buffer

	UniformBuffer uniformBuffer{ camera->position, camera->right, camera->up, camera->front, 0 };

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, sizeof(UniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, uniformBufferBuffer, uniformBufferMemory);

	void* uniformBufferMemPtr;
	vkMapMemory(logicalDevice, uniformBufferMemory, 0, sizeof(UniformBuffer), 0, &uniformBufferMemPtr);
	memcpy(uniformBufferMemPtr, &uniformBuffer, sizeof(UniformBuffer));

	Vulkan::globalThreadingMutex->unlock();
	
	//Destroy(logicalDevice);
}


void RayTracing::DrawFrame(Win32Window* window, Camera* camera)
{
	/*image = new Image();
	image->GenerateEmptyImages(creationObject, window->GetWidth(), window->GetHeight(), 1);*/

	// image
	
	VkImage RTImage;
	VkDeviceMemory RTImageMemory;

	Vulkan::CreateImage(logicalDevice, physicalDevice, window->GetWidth(), window->GetHeight(), 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, RTImage, RTImageMemory);
	VkImageView RTImageView = Vulkan::CreateImageView(logicalDevice, RTImage, VK_IMAGE_VIEW_TYPE_2D, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

	VkImageMemoryBarrier RTImageMemoryBarrier{};
	RTImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	RTImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	RTImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	RTImageMemoryBarrier.srcAccessMask = 0;
	RTImageMemoryBarrier.dstAccessMask = 0;
	RTImageMemoryBarrier.srcQueueFamilyIndex = queueFamilyIndex;
	RTImageMemoryBarrier.dstQueueFamilyIndex = queueFamilyIndex;
	RTImageMemoryBarrier.image = RTImage;
	RTImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	RTImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	RTImageMemoryBarrier.subresourceRange.levelCount = 1;
	RTImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	RTImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkCommandBuffer imageBarrierCommandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);
	vkCmdPipelineBarrier(imageBarrierCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &RTImageMemoryBarrier);
	Vulkan::EndSingleTimeCommands(logicalDevice, queue, imageBarrierCommandBuffer, commandPool);

	// descriptor infos

	VkWriteDescriptorSetAccelerationStructureKHR ASDescriptorInfo{};
	ASDescriptorInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	ASDescriptorInfo.accelerationStructureCount = 1;
	ASDescriptorInfo.pAccelerationStructures = &TLAS;

	VkDescriptorBufferInfo uniformDescriptorInfo{};
	uniformDescriptorInfo.buffer = uniformBufferBuffer;
	uniformDescriptorInfo.offset = 0;
	uniformDescriptorInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo indexDescriptorInfo{};
	indexDescriptorInfo.buffer = testObjectIndexBuffer.GetVkBuffer();
	indexDescriptorInfo.offset = 0;
	indexDescriptorInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo vertexDescriptorInfo{};
	vertexDescriptorInfo.buffer = testObjectVertexBuffer.GetVkBuffer();
	vertexDescriptorInfo.offset = 0;
	vertexDescriptorInfo.range = VK_WHOLE_SIZE;
	
	VkDescriptorImageInfo RTImageDescriptorImageInfo{};
	RTImageDescriptorImageInfo.sampler = VK_NULL_HANDLE;
	RTImageDescriptorImageInfo.imageView = RTImageView;//image->imageView;
	RTImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	std::vector<VkWriteDescriptorSet> writeDescriptorSets(5);

	writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	writeDescriptorSets[0].pNext = &ASDescriptorInfo;
	writeDescriptorSets[0].dstSet = descriptorSets[0];
	writeDescriptorSets[0].descriptorCount = 1;
	writeDescriptorSets[0].dstBinding = 0;
	
	writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptorSets[1].pNext = &ASDescriptorInfo;
	writeDescriptorSets[1].dstSet = descriptorSets[0];
	writeDescriptorSets[1].descriptorCount = 1;
	writeDescriptorSets[1].dstBinding = 1;
	writeDescriptorSets[1].pBufferInfo = &uniformDescriptorInfo;

	writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeDescriptorSets[2].pNext = &ASDescriptorInfo;
	writeDescriptorSets[2].dstSet = descriptorSets[0];
	writeDescriptorSets[2].descriptorCount = 1;
	writeDescriptorSets[2].dstBinding = 2;
	writeDescriptorSets[2].pBufferInfo = &indexDescriptorInfo;

	writeDescriptorSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSets[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeDescriptorSets[3].pNext = &ASDescriptorInfo;
	writeDescriptorSets[3].dstSet = descriptorSets[0];
	writeDescriptorSets[3].descriptorCount = 1;
	writeDescriptorSets[3].dstBinding = 3;
	writeDescriptorSets[3].pBufferInfo = &vertexDescriptorInfo;

	writeDescriptorSets[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSets[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeDescriptorSets[4].pNext = &ASDescriptorInfo;
	writeDescriptorSets[4].dstSet = descriptorSets[0];
	writeDescriptorSets[4].descriptorCount = 1;
	writeDescriptorSets[4].dstBinding = 4;
	writeDescriptorSets[4].pImageInfo = &RTImageDescriptorImageInfo;
	
	vkUpdateDescriptorSets(logicalDevice, (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);

	// material index list

	std::vector<uint32_t> materialIndices;
	materialIndices.push_back(0);

	VkBuffer materialIndexBuffer;
	VkDeviceMemory materialIndexBufferMemory;
	VkDeviceSize materialIndexBufferSize = sizeof(uint32_t) * materialIndices.size();

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, materialIndexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, materialIndexBuffer, materialIndexBufferMemory);

	void* materialIndexBufferMemPtr;
	VkResult result = vkMapMemory(logicalDevice, materialIndexBufferMemory, 0, materialIndexBufferSize, 0, &materialIndexBufferMemPtr);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to map the ray tracing material index buffer memory", result, nameof(vkMapMemory), __FILENAME__, std::to_string(__LINE__));

	memcpy(materialIndexBufferMemPtr, materialIndices.data(), materialIndexBufferSize);
	vkUnmapMemory(logicalDevice, materialIndexBufferMemory);

	// material buffer

	struct Material 
	{
		float ambient[4] = { 0.2f, 0.2f, 0.2f, 0.2f };
		float diffuse[4] = { 0.5f, 0.3f, 0.7f, 1.0f };
		float specular[4] = { 0.3f, 0.3f, 0.3f, 0.3f };
		float emission[4] = { 0, 0, 0, 0 };
	};

	std::vector<Material> materials(1);

	VkBuffer materialBuffer;
	VkDeviceMemory materialBufferMemory;
	VkDeviceSize materialBufferSize = sizeof(Material) * materials.size();

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, materialBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, materialBuffer, materialBufferMemory);

	void* materialsBufferMemPtr;
	result = vkMapMemory(logicalDevice, materialBufferMemory, 0, materialBufferSize, 0, &materialsBufferMemPtr);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to map the ray tracing material buffer memory", result, nameof(vkMapMemory), __FILENAME__, std::to_string(__LINE__));

	memcpy(materialsBufferMemPtr, materials.data(), materialBufferSize);
	vkUnmapMemory(logicalDevice, materialBufferMemory);

	// update material descriptor set

	VkDescriptorBufferInfo materialIndexDescriptorInfo{};
	materialIndexDescriptorInfo.buffer = materialIndexBuffer;
	materialIndexDescriptorInfo.offset = 0;
	materialIndexDescriptorInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo materialBufferDescriptorInfo{};
	materialBufferDescriptorInfo.buffer = materialBuffer;
	materialBufferDescriptorInfo.offset = 0;
	materialBufferDescriptorInfo.range = VK_WHOLE_SIZE;

	std::vector<VkWriteDescriptorSet> materialWriteDescriptorSets(2);

	materialWriteDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	materialWriteDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialWriteDescriptorSets[0].dstSet = descriptorSets[1];
	materialWriteDescriptorSets[0].dstBinding = 0;
	materialWriteDescriptorSets[0].descriptorCount = 1;
	materialWriteDescriptorSets[0].pBufferInfo = &materialIndexDescriptorInfo;

	materialWriteDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	materialWriteDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialWriteDescriptorSets[1].dstSet = descriptorSets[1];
	materialWriteDescriptorSets[1].dstBinding = 1;
	materialWriteDescriptorSets[1].descriptorCount = 1;
	materialWriteDescriptorSets[1].pBufferInfo = &materialBufferDescriptorInfo;

	vkUpdateDescriptorSets(logicalDevice, (uint32_t)materialWriteDescriptorSets.size(), materialWriteDescriptorSets.data(), 0, nullptr);

	// shader binding table

	VkDeviceSize progSize = rayTracingProperties.shaderGroupBaseAlignment;
	VkDeviceSize shaderBindingTableSize = progSize * 4;

	VkBuffer shaderBindingTableBuffer;
	VkDeviceMemory shaderBindingTableMemory;

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, shaderBindingTableSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, shaderBindingTableBuffer, shaderBindingTableMemory);

	std::vector<char> shaderBuffer(shaderBindingTableSize);
	result = pvkGetRayTracingShaderGroupHandlesKHR(logicalDevice, pipeline, 0, 4, shaderBindingTableSize, shaderBuffer.data());
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to get the ray tracing shader group handles", result, nameof(pvkGetRayTracingShaderGroupHandlesKHR), __FILENAME__, std::to_string(__LINE__));

	void* shaderBindingTableMemPtr;
	result = vkMapMemory(logicalDevice, shaderBindingTableMemory, 0, shaderBindingTableSize, 0, &shaderBindingTableMemPtr);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to map the shader binding table memory", result, nameof(vkMapMemory), __FILENAME__, std::to_string(__LINE__));

	for (uint32_t i = 0; i < 4; i++) // 4 = amount of shaders
	{
		memcpy(shaderBindingTableMemPtr, shaderBuffer.data() + i * rayTracingProperties.shaderGroupHandleSize, rayTracingProperties.shaderGroupHandleSize);
		shaderBindingTableMemPtr = static_cast<char*>(shaderBindingTableMemPtr) + rayTracingProperties.shaderGroupBaseAlignment;
	}
	vkUnmapMemory(logicalDevice, shaderBindingTableMemory);

	VkBufferDeviceAddressInfo shaderBindingTableBufferAddressInfo{};
	shaderBindingTableBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	shaderBindingTableBufferAddressInfo.buffer = shaderBindingTableBuffer;

	VkDeviceAddress shaderBindingTableBufferAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &shaderBindingTableBufferAddressInfo);

	VkDeviceSize hitGroupOffset = 0;
	VkDeviceSize rayGenOffset = progSize;
	VkDeviceSize missOffset = 2 * progSize;

	VkStridedDeviceAddressRegionKHR rchitShaderBindingTable{};
	rchitShaderBindingTable.deviceAddress = shaderBindingTableBufferAddress + hitGroupOffset;
	rchitShaderBindingTable.size = progSize;
	rchitShaderBindingTable.stride = progSize;

	VkStridedDeviceAddressRegionKHR rgenShaderBindingTable{};
	rgenShaderBindingTable.deviceAddress = shaderBindingTableBufferAddress + rayGenOffset;
	rgenShaderBindingTable.size = progSize;
	rgenShaderBindingTable.stride = progSize;

	VkStridedDeviceAddressRegionKHR rmissShaderBindingTable{};
	rmissShaderBindingTable.deviceAddress = shaderBindingTableBufferAddress + missOffset;
	rmissShaderBindingTable.size = progSize;
	rmissShaderBindingTable.stride = progSize;

	VkStridedDeviceAddressRegionKHR callableShaderBindingTable{};

	// end buffer for the image

	VkBuffer resultBuffer;
	VkDeviceMemory resultBufferMemory;

	VkMemoryRequirements RTMemRequirements;
	vkGetImageMemoryRequirements(logicalDevice, RTImage/*image->image*/, &RTMemRequirements);

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, RTMemRequirements.size/*window->GetWidth() * window->GetHeight() * 4*/, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, resultBuffer, resultBufferMemory); // not sure about the size
	
	// record the render pass command buffer

	VkImageMemoryBarrier RTCopyMemoryBarrier{};
	RTCopyMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	//RTCopyMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	RTCopyMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	RTCopyMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	RTCopyMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	RTCopyMemoryBarrier.srcQueueFamilyIndex = queueFamilyIndex;
	RTCopyMemoryBarrier.dstQueueFamilyIndex = queueFamilyIndex;
	RTCopyMemoryBarrier.image = RTImage;/*image->image*/
	RTCopyMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	RTCopyMemoryBarrier.subresourceRange.baseMipLevel = 0;
	RTCopyMemoryBarrier.subresourceRange.levelCount = 1;
	RTCopyMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	RTCopyMemoryBarrier.subresourceRange.layerCount = 1;

	VkBufferImageCopy imageCopy{};
	imageCopy.bufferOffset = 0;
	imageCopy.bufferRowLength = 0;
	imageCopy.bufferImageHeight = 0;
	imageCopy.imageOffset = { 0, 0, 0 };
	imageCopy.imageExtent.width = window->GetWidth();
	imageCopy.imageExtent.height = window->GetHeight();
	imageCopy.imageExtent.depth = 1;
	imageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopy.imageSubresource.baseArrayLayer = 0;
	imageCopy.imageSubresource.mipLevel = 0;
	imageCopy.imageSubresource.layerCount = 1;

	VkCommandBufferBeginInfo commandBufferBeginInfo{};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

	result = vkBeginCommandBuffer(commandBuffers[0], &commandBufferBeginInfo);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to begin the ray tracing render pass command buffer", result, nameof(vkBeginCommandBuffer), __FILENAME__, std::to_string(__LINE__));
	
	vkCmdBindPipeline(commandBuffers[0], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
	vkCmdBindDescriptorSets(commandBuffers[0], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	pvkCmdTraceRaysKHR(commandBuffers[0], &rgenShaderBindingTable, &rmissShaderBindingTable, &rchitShaderBindingTable, &callableShaderBindingTable, window->GetWidth(), window->GetHeight(), 1);
	vkCmdPipelineBarrier(commandBuffers[0], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &RTCopyMemoryBarrier);
	vkCmdCopyImageToBuffer(commandBuffers[0], RTImage/*image->image*/, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, resultBuffer, 1, &imageCopy);
	
	result = vkEndCommandBuffer(commandBuffers[0]);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to end the ray tracing render pass command buffer", result, nameof(vkBeginCommandBuffer), __FILENAME__, std::to_string(__LINE__));

	// fence

	VkFence imageFence;

	VkFenceCreateInfo imageFenceCreateInfo{};
	imageFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	imageFenceCreateInfo.flags = 0;

	result = vkCreateFence(logicalDevice, &imageFenceCreateInfo, nullptr, &imageFence);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create a ray tracing fence", result, nameof(vkCreateFence), __FILENAME__, std::to_string(__LINE__));

	// submit

	VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitDstStageMask = &pipelineStageFlags;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[0];
	submitInfo.signalSemaphoreCount = 0;

	result = vkQueueSubmit(queue, 1, &submitInfo, imageFence);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to submit the ray tracing queue", result, nameof(vkQueueSubmit), __FILENAME__, std::to_string(__LINE__));
	
	// read the image
	
	void* resultMemPtr;
	result = vkMapMemory(logicalDevice, resultBufferMemory, 0, RTMemRequirements.size, 0, &resultMemPtr);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to map the image memory", result, nameof(vkMapMemory), __FILENAME__, std::to_string(__LINE__));

	stbi_write_png("result.png", window->GetWidth(), window->GetHeight(), 4, resultMemPtr, 4 * window->GetHeight());
	/*vkUnmapMemory(logicalDevice, resultBufferMemory);

	vkFreeMemory(logicalDevice, resultBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, resultBuffer, nullptr);
	vkDestroyFence(logicalDevice, imageFence, nullptr);*/

	std::cout << "end" << std::endl;
}