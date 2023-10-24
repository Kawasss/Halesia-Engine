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
#include "renderer/renderer.h"
#include "system/Input.h"

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

VkBuffer TLASBuffer;
VkDeviceMemory TLASBufferMemory;
VkAccelerationStructureKHR TLAS;

VkBuffer TLASScratchBuffer;
VkDeviceMemory TLASScratchMemory;

VkBuffer uniformBufferBuffer;
VkDeviceMemory uniformBufferMemory;

uint32_t facesCount;
uint32_t verticesSize;
uint32_t indicesSize;
VulkanBuffer testObjectVertexBuffer;
IndexBuffer testObjectIndexBuffer;

struct UniformBuffer
{
	/*glm::vec3 position;
	glm::vec3 right;
	glm::vec3 up;
	glm::vec3 forward;

	uint32_t frameCount;*/
	float cameraPosition[4] = { -1.433908, 3.579997, 5.812919, 1 };
	float cameraRight[4] = { 0.928479, 0, 0.371385, 1 };
	float cameraUp[4] = { 0, 1, 0, 1 };
	float cameraForward[4] = { 0.371385, 0, -0.928479, 1 };

	uint32_t frameCount = 0;
	int32_t showPrimitiveID = 0;
	uint32_t faceCount = 0;
};
void* uniformBufferMemPtr;
UniformBuffer uniformBuffer{};

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
	const aiScene* scene = aiImportFile("stdObj/monkey3.obj", aiProcessPreset_TargetRealtime_Fast);
	if (scene == nullptr)
		throw std::runtime_error("Failed to read the test model");

	std::vector</*glm::vec3*/float> vertices;
	std::vector<uint16_t> indices;

	for (int i = 0; i < scene->mMeshes[0]->mNumVertices; i++)
	{
		vertices.push_back(scene->mMeshes[0]->mVertices[i].x);
		vertices.push_back(scene->mMeshes[0]->mVertices[i].y);
		vertices.push_back(scene->mMeshes[0]->mVertices[i].z);
	}
		
	for (int i = 0; i < scene->mMeshes[0]->mNumFaces; i++)
		for (int j = 0; j < scene->mMeshes[0]->mFaces[i].mNumIndices; j++)
			indices.push_back(scene->mMeshes[0]->mFaces[i].mIndices[j]);

	testObjectVertexBuffer = VulkanBuffer(creationObject, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vertices);
	testObjectIndexBuffer = IndexBuffer(creationObject, indices);
	verticesSize = vertices.size();
	indicesSize = indices.size();
	facesCount = scene->mMeshes[0]->mNumFaces;
}

void RayTracing::Destroy(VkDevice logicalDevice)
{
	testObjectIndexBuffer.Destroy();
	testObjectVertexBuffer.Destroy();

	vkFreeMemory(logicalDevice, uniformBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, uniformBufferBuffer, nullptr);

	vkFreeMemory(logicalDevice, TLASScratchMemory, nullptr);
	vkDestroyBuffer(logicalDevice, TLASScratchBuffer, nullptr);

	vkDestroyAccelerationStructureKHR(logicalDevice, TLAS, nullptr);

	vkFreeMemory(logicalDevice, TLASBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, TLASBuffer, nullptr);

	vkFreeMemory(logicalDevice, BLAS.geometryInstanceBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, BLAS.geometryInstanceBuffer, nullptr);

	vkDestroyAccelerationStructureKHR(logicalDevice, BLAS.accelerationStructure, nullptr);

	vkDestroyBuffer(logicalDevice, BLAS.scratchBuffer, nullptr);
	vkFreeMemory(logicalDevice, BLAS.scratchDeviceMemory, nullptr);

	vkDestroyBuffer(logicalDevice, BLAS.buffer, nullptr);
	vkFreeMemory(logicalDevice, BLAS.deviceMemory, nullptr);

	vkDestroyPipeline(logicalDevice, pipeline, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);

	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, materialSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);

	vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
}

void RayTracing::CreateBLAS(BottomLevelAccelerationStructure& BLAS, VulkanBuffer vertexBuffer, IndexBuffer indexBuffer, uint32_t vertexSize, uint32_t faceCount)
{
	VkBufferDeviceAddressInfo bufferAddressInfo{};
	bufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferAddressInfo.buffer = vertexBuffer.GetVkBuffer();

	VkDeviceAddress vertexBufferAddress = vkGetBufferDeviceAddressKHR(logicalDevice, &bufferAddressInfo);

	bufferAddressInfo.buffer = indexBuffer.GetVkBuffer();

	VkDeviceAddress indexBufferAddress = vkGetBufferDeviceAddressKHR(logicalDevice, &bufferAddressInfo);

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData = { vertexBufferAddress };
	triangles.vertexStride = sizeof(float) * 3;
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
	vkGetAccelerationStructureBuildSizesKHR(logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &BLASBuildGeometryInfo, BLMaxPrimitiveCounts.data(), &BLASBuildSizesInfo);

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, BLASBuildSizesInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, BLAS.buffer, BLAS.deviceMemory);

	VkAccelerationStructureCreateInfoKHR BLASCreateInfo{};
	BLASCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	BLASCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	BLASCreateInfo.size = BLASBuildSizesInfo.accelerationStructureSize;
	BLASCreateInfo.buffer = BLAS.buffer;
	BLASCreateInfo.offset = 0;

	VkResult result = vkCreateAccelerationStructureKHR(logicalDevice, &BLASCreateInfo, nullptr, &BLAS.accelerationStructure);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the BLAS", result, nameof(vkCreateAccelerationStructureKHR), __FILENAME__, __STRLINE__);

	// build BLAS

	VkAccelerationStructureDeviceAddressInfoKHR BLASAddressInfo{};
	BLASAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	BLASAddressInfo.accelerationStructure = BLAS.accelerationStructure;

	BLAS.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &BLASAddressInfo);

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, BLASBuildSizesInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, BLAS.scratchBuffer, BLAS.scratchDeviceMemory);

	VkBufferDeviceAddressInfo scratchDeviceAddressInfo{};
	scratchDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	scratchDeviceAddressInfo.buffer = BLAS.scratchBuffer;

	VkDeviceAddress scratchDeviceAddress = vkGetBufferDeviceAddress(logicalDevice, &scratchDeviceAddressInfo);

	BLASBuildGeometryInfo.scratchData = { scratchDeviceAddress };
	BLASBuildGeometryInfo.dstAccelerationStructure = BLAS.accelerationStructure;

	VkAccelerationStructureBuildRangeInfoKHR BLASBuildRangeInfo{};
	BLASBuildRangeInfo.primitiveCount = faceCount;
	const VkAccelerationStructureBuildRangeInfoKHR* pBLASBuildRangeInfo = &BLASBuildRangeInfo;

	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);
	vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &BLASBuildGeometryInfo, &pBLASBuildRangeInfo);
	Vulkan::EndSingleTimeCommands(logicalDevice, queue, commandBuffer, commandPool);
}

void RayTracing::Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Object* object, Camera* camera, Win32Window* window, Swapchain* swapchain)
{
	VkResult result = VK_SUCCESS;

	this->logicalDevice = logicalDevice;
	this->physicalDevice = physicalDevice;
	this->swapchain = swapchain;
	this->window = window;

	rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

	VkPhysicalDeviceProperties2 properties2{};
	properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &rayTracingProperties;

	vkGetPhysicalDeviceProperties2(physicalDevice.Device(), &properties2);

	if (!physicalDevice.QueueFamilies(surface).graphicsFamily.has_value())
		throw VulkanAPIError("No appropriate graphics family could be found for ray tracing", VK_SUCCESS, nameof(physicalDevice.QueueFamilies(surface).graphicsFamily.has_value()), __FILENAME__, std::to_string(__LINE__));

	queueFamilyIndex = physicalDevice.QueueFamilies(surface).graphicsFamily.value();
	vkGetDeviceQueue(logicalDevice, queueFamilyIndex, 0, &queue);

	// creation object

	creationObject = { logicalDevice, physicalDevice, /*commandPool,*/ queue, queueFamilyIndex };
	CreateTestObject(creationObject);
	commandPool = Vulkan::FetchNewCommandPool(creationObject);
	//Vulkan::graphicsQueueMutex->lock();

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
	descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
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

	std::vector<VkDescriptorBindingFlags> setBindingFlags;
	setBindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	setBindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	setBindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	setBindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	setBindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);

	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setBindingFlagsCreateInfo{};
	setBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	setBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(setBindingFlags.size());
	setBindingFlagsCreateInfo.pBindingFlags = setBindingFlags.data();

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	layoutCreateInfo.pBindings = setLayoutBindings.data();
	layoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layoutCreateInfo.pNext = &setBindingFlagsCreateInfo;

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

	std::vector<VkDescriptorBindingFlags> bindingFlags;
	bindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	bindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);

	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagsCreateInfo{};
	bindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	bindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(materialLayoutBindings.size());
	bindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

	VkDescriptorSetLayoutCreateInfo materialLayoutCreateInfo{};
	materialLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	materialLayoutCreateInfo.bindingCount = static_cast<uint32_t>(materialLayoutBindings.size());
	materialLayoutCreateInfo.pBindings = materialLayoutBindings.data();
	materialLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	materialLayoutCreateInfo.pNext = &bindingFlagsCreateInfo;

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

	result = vkCreateRayTracingPipelinesKHR(logicalDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &RTPipelineCreateInfo, nullptr, &pipeline);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the pipeline for ray tracing", result, nameof(vkCreateRayTracingPipelinesKHR), __FILENAME__, std::to_string(__LINE__));

	vkDestroyShaderModule(logicalDevice, shadowShader, nullptr);
	vkDestroyShaderModule(logicalDevice, missShader, nullptr);
	vkDestroyShaderModule(logicalDevice, hitShader, nullptr);
	vkDestroyShaderModule(logicalDevice, genShader, nullptr);

	// bottom level acceleration structure

	CreateBLAS(BLAS, testObjectVertexBuffer, testObjectIndexBuffer, verticesSize, facesCount);
	
	// TLAS

	VkAccelerationStructureInstanceKHR BLASInstance{};
	BLASInstance.transform = {  1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
	BLASInstance.instanceCustomIndex = 0;
	BLASInstance.mask = 0xFF;
	BLASInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	BLASInstance.accelerationStructureReference = BLAS.deviceAddress;

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, sizeof(VkAccelerationStructureInstanceKHR), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, BLAS.geometryInstanceBuffer, BLAS.geometryInstanceBufferMemory);
	
	void* BLGeometryInstanceBufferMemPtr;

	result = vkMapMemory(logicalDevice, BLAS.geometryInstanceBufferMemory, 0, sizeof(VkAccelerationStructureInstanceKHR), 0, &BLGeometryInstanceBufferMemPtr);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to map the memory of the bottom level geometry instance buffer", result, nameof(vkMapMemory), __FILENAME__, std::to_string(__LINE__));

	memcpy(BLGeometryInstanceBufferMemPtr, &BLASInstance, sizeof(VkAccelerationStructureInstanceKHR));
	vkUnmapMemory(logicalDevice, BLAS.geometryInstanceBufferMemory);

	VkBufferDeviceAddressInfo BLGeometryInstanceAddressInfo{};
	BLGeometryInstanceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	BLGeometryInstanceAddressInfo.buffer = BLAS.geometryInstanceBuffer;

	VkDeviceAddress BLGeometryInstanceAddress = vkGetBufferDeviceAddressKHR(logicalDevice, &BLGeometryInstanceAddressInfo);

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

	vkGetAccelerationStructureBuildSizesKHR(logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &TLASBuildGeometryInfo, TLASMaxPrimitiveCounts.data(), &TLASBuildSizesInfo);

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, TLASBuildSizesInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TLASBuffer, TLASBufferMemory);

	VkAccelerationStructureCreateInfoKHR TLASCreateInfo{};
	TLASCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	TLASCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	TLASCreateInfo.size = TLASBuildSizesInfo.accelerationStructureSize;
	TLASCreateInfo.buffer = TLASBuffer;
	TLASCreateInfo.deviceAddress = 0;

	result = vkCreateAccelerationStructureKHR(logicalDevice, &TLASCreateInfo, nullptr, &TLAS);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the TLAS", result, nameof(vkCreateAccelerationStructureKHR), __FILENAME__, std::to_string(__LINE__));

	// build TLAS

	VkAccelerationStructureDeviceAddressInfoKHR TLASAddressInfo{};
	TLASAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	TLASAddressInfo.accelerationStructure = TLAS;

	VkDeviceAddress TLASDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &TLASAddressInfo);

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, TLASBuildSizesInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TLASScratchBuffer, TLASScratchMemory);

	VkBufferDeviceAddressInfo TLASSratchBufferAddressInfo{};
	TLASSratchBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	TLASSratchBufferAddressInfo.buffer = TLASScratchBuffer;

	VkDeviceAddress TLASScratchBufferAddress = vkGetBufferDeviceAddressKHR(logicalDevice, &TLASSratchBufferAddressInfo);

	TLASBuildGeometryInfo.dstAccelerationStructure = TLAS;
	TLASBuildGeometryInfo.scratchData = { TLASScratchBufferAddress };

	VkAccelerationStructureBuildRangeInfoKHR TLASBuildRangeInfo{};
	TLASBuildRangeInfo.primitiveCount = 1;
	const VkAccelerationStructureBuildRangeInfoKHR* pTLASBuildRangeInfos = &TLASBuildRangeInfo;
	
	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);
	vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &TLASBuildGeometryInfo, &pTLASBuildRangeInfos);
	Vulkan::EndSingleTimeCommands(logicalDevice, queue, commandBuffer, commandPool);

	// uniform buffer

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, sizeof(UniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, uniformBufferBuffer, uniformBufferMemory);

	vkMapMemory(logicalDevice, uniformBufferMemory, 0, sizeof(UniformBuffer), 0, &uniformBufferMemPtr);
	memcpy(uniformBufferMemPtr, &uniformBuffer, sizeof(UniformBuffer));

	// fence

	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	result = vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &fence);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create a ray tracing fence", result, nameof(vkCreateFence), __FILENAME__, __STRLINE__);

	//Vulkan::graphicsQueueMutex->unlock();
	
	// semaphore

	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	result = vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &imageSemaphore);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the ray tracing semaphore", result, nameof(vkCreateSemaphore), __FILENAME__, __STRLINE__);
	result = vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &renderSemaphore);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the ray tracing semaphore", result, nameof(vkCreateSemaphore), __FILENAME__, __STRLINE__);

	CreateShaderBindingTable();

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

	std::vector<VkWriteDescriptorSet> writeDescriptorSets(4);

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

	vkUpdateDescriptorSets(logicalDevice, (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);

	CreateMaterialBuffers();

	UpdateMaterialDescriptorSets();

	CreateImage(swapchain->extent.width, swapchain->extent.height);
}

void RayTracing::CreateMaterialBuffers()
{
	std::vector<uint32_t> materialIndices;
	materialIndices.push_back(0);
	materialIndices.push_back(0);

	for (int i = 0; i < facesCount; i++)
	{
		if (i == 12 || i == 13)
			materialIndices.push_back(0);
		else if (i < 14)
			materialIndices.push_back(1);
		else
			materialIndices.push_back(2);
	}
		

	VkDeviceSize materialIndexBufferSize = sizeof(uint16_t) * materialIndices.size();

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, materialIndexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, materialIndexBuffer, materialIndexBufferMemory);

	void* materialIndexBufferMemPtr;
	VkResult result = vkMapMemory(logicalDevice, materialIndexBufferMemory, 0, materialIndexBufferSize, 0, &materialIndexBufferMemPtr);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to map the ray tracing material index buffer memory", result, nameof(vkMapMemory), __FILENAME__, std::to_string(__LINE__));

	memcpy(materialIndexBufferMemPtr, materialIndices.data(), materialIndexBufferSize);
	vkUnmapMemory(logicalDevice, materialIndexBufferMemory);

	struct Material
	{
		float ambient[3] = { 0.2f, 0.2f, 0.2f };
		float diffuse[3] = { 0.5f, 0.0f, 0.7f };
		float specular[3] = { 0.3f, 0.3f, 0.3f };
		float emission[3] = { 1, 1, 1 };
	};

	std::vector<Material> materials;
	materials.push_back(Material{ { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 } });
	materials.push_back(Material{ { 0.2f, 0.2f, 0.2f }, { 0.8f, 0.8f, 0.8f }, { 0.3f, 0.3f, 0.3f }, { 0.2f, 0.2f, 0.2f } });
	materials.push_back(Material{ { 0.2f, 0.2f, 0.2f }, { 0.5f, 0.0f, 0.7f }, { 0.3f, 0.3f, 0.3f }, { 0.5f, 0.5f, 0.5f } });
	
	
	

	VkDeviceSize materialBufferSize = sizeof(Material) * materials.size();

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, materialBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, materialBuffer, materialBufferMemory);

	void* materialsBufferMemPtr;
	result = vkMapMemory(logicalDevice, materialBufferMemory, 0, materialBufferSize, 0, &materialsBufferMemPtr);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to map the ray tracing material buffer memory", result, nameof(vkMapMemory), __FILENAME__, std::to_string(__LINE__));

	memcpy(materialsBufferMemPtr, materials.data(), materialBufferSize);
	vkUnmapMemory(logicalDevice, materialBufferMemory);
}

void RayTracing::UpdateMaterialDescriptorSets()
{
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
}

void RayTracing::CreateImage(uint32_t width, uint32_t height)
{
	if (RTImage != VK_NULL_HANDLE && RTImageMemory != VK_NULL_HANDLE && RTImageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(logicalDevice, RTImageView, nullptr);
		vkDestroyImage(logicalDevice, RTImage, nullptr);
		vkFreeMemory(logicalDevice, RTImageMemory, nullptr);
	}

	Vulkan::CreateImage(logicalDevice, physicalDevice, width, height, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, RTImage, RTImageMemory);
	RTImageView = Vulkan::CreateImageView(logicalDevice, RTImage, VK_IMAGE_VIEW_TYPE_2D, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

	VkImageMemoryBarrier RTImageMemoryBarrier{};
	RTImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	RTImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	RTImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	RTImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	RTImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	RTImageMemoryBarrier.image = RTImage;
	RTImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	RTImageMemoryBarrier.subresourceRange.levelCount = 1;
	RTImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkCommandBuffer imageBarrierCommandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);
	vkCmdPipelineBarrier(imageBarrierCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &RTImageMemoryBarrier);
	Vulkan::EndSingleTimeCommands(logicalDevice, queue, imageBarrierCommandBuffer, commandPool);
}

VkStridedDeviceAddressRegionKHR rchitShaderBindingTable{};
VkStridedDeviceAddressRegionKHR rgenShaderBindingTable{};
VkStridedDeviceAddressRegionKHR rmissShaderBindingTable{};
VkStridedDeviceAddressRegionKHR callableShaderBindingTable{};

void RayTracing::CreateShaderBindingTable()
{
	VkDeviceSize progSize = rayTracingProperties.shaderGroupBaseAlignment;
	VkDeviceSize shaderBindingTableSize = progSize * 4;

	VkBuffer shaderBindingTableBuffer;
	VkDeviceMemory shaderBindingTableMemory;

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, shaderBindingTableSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, shaderBindingTableBuffer, shaderBindingTableMemory);

	std::vector<char> shaderBuffer(shaderBindingTableSize);
	VkResult result = vkGetRayTracingShaderGroupHandlesKHR(logicalDevice, pipeline, 0, 4, shaderBindingTableSize, shaderBuffer.data());
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to get the ray tracing shader group handles", result, nameof(vkGetRayTracingShaderGroupHandlesKHR), __FILENAME__, __STRLINE__);

	void* shaderBindingTableMemPtr;
	result = vkMapMemory(logicalDevice, shaderBindingTableMemory, 0, shaderBindingTableSize, 0, &shaderBindingTableMemPtr);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to map the shader binding table memory", result, nameof(vkMapMemory), __FILENAME__, __STRLINE__);

	for (uint32_t i = 0; i < 4; i++) // 4 = amount of shaders
	{
		memcpy(shaderBindingTableMemPtr, shaderBuffer.data() + i * rayTracingProperties.shaderGroupHandleSize, rayTracingProperties.shaderGroupHandleSize);
		shaderBindingTableMemPtr = static_cast<char*>(shaderBindingTableMemPtr) + rayTracingProperties.shaderGroupBaseAlignment;
	}
	vkUnmapMemory(logicalDevice, shaderBindingTableMemory);

	VkBufferDeviceAddressInfo shaderBindingTableBufferAddressInfo{};
	shaderBindingTableBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	shaderBindingTableBufferAddressInfo.buffer = shaderBindingTableBuffer;

	VkDeviceAddress shaderBindingTableBufferAddress = vkGetBufferDeviceAddressKHR(logicalDevice, &shaderBindingTableBufferAddressInfo);

	VkDeviceSize hitGroupOffset = 0;
	VkDeviceSize rayGenOffset = progSize;
	VkDeviceSize missOffset = progSize * 2;

	rchitShaderBindingTable.deviceAddress = shaderBindingTableBufferAddress + hitGroupOffset;
	rchitShaderBindingTable.size = progSize;
	rchitShaderBindingTable.stride = progSize;

	rgenShaderBindingTable.deviceAddress = shaderBindingTableBufferAddress + rayGenOffset;
	rgenShaderBindingTable.size = progSize;
	rgenShaderBindingTable.stride = progSize;

	rmissShaderBindingTable.deviceAddress = shaderBindingTableBufferAddress + missOffset;
	rmissShaderBindingTable.size = progSize;
	rmissShaderBindingTable.stride = progSize;
}

uint32_t frameCount = 0;
void RayTracing::DrawFrame(Win32Window* window, Camera* camera, Swapchain* swapchain, Surface surface, VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkWriteDescriptorSetAccelerationStructureKHR ASDescriptorInfo{};
	ASDescriptorInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	ASDescriptorInfo.accelerationStructureCount = 1;
	ASDescriptorInfo.pAccelerationStructures = &TLAS;

	VkDescriptorImageInfo RTImageDescriptorImageInfo{};
	RTImageDescriptorImageInfo.sampler = VK_NULL_HANDLE;
	RTImageDescriptorImageInfo.imageView = RTImageView;
	RTImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkWriteDescriptorSet writeDescriptorSet{};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeDescriptorSet.pNext = &ASDescriptorInfo;
	writeDescriptorSet.dstSet = descriptorSets[0];
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.dstBinding = 4;
	writeDescriptorSet.pImageInfo = &RTImageDescriptorImageInfo;

	vkUpdateDescriptorSets(logicalDevice, 1, &writeDescriptorSet, 0, nullptr);

	uniformBuffer = UniformBuffer{ { camera->position.x, camera->position.y, camera->position.z, 1 }, { camera->right.x, camera->right.y, camera->right.z, 1 }, { camera->up.x, camera->up.y, camera->up.z, 1 }, { camera->front.x, camera->front.y, camera->front.z, 1 }, frameCount, 0, facesCount };
	//uniformBuffer = UniformBuffer{ { 7.24205f, -4.13095f, 7.67253f, 1 }, { 0.70373f, 0.00000f, -0.71047f, 1 }, { -0.28477f, 0.91616f, -0.28206f, 1 }, { -0.65091f, -0.40081f, -0.64473f, 1 }, frameCount };
	//printf("pos: %.5f, %.5f, %.5f, right: %.5f, %.5f, %.5f, up: %.5f, %.5f, %.5f, front: %.5f, %.5f, %.5f\n", camera->position.x, camera->position.y, camera->position.z, camera->right.x, camera->right.y, camera->right.z, camera->up.x, camera->up.y, camera->up.z, camera->front.x, camera->front.y, camera->front.z);
	//std::cout << facesCount << std::endl;

	int x, y;
	window->GetRelativeCursorPosition(x, y);
	if (x != 0 && y != 0 || Input::IsKeyPressed(VirtualKey::W) || Input::IsKeyPressed(VirtualKey::A) || Input::IsKeyPressed(VirtualKey::S) || Input::IsKeyPressed(VirtualKey::D))
		frameCount = 0;

	memcpy(uniformBufferMemPtr, &uniformBuffer, sizeof(UniformBuffer));

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	vkCmdTraceRaysKHR(commandBuffer, &rgenShaderBindingTable, &rmissShaderBindingTable, &rchitShaderBindingTable, &callableShaderBindingTable, swapchain->extent.width, swapchain->extent.height, 1);

	frameCount++;
}