#include <fstream>
#include "renderer/RayTracing.h"
#include "renderer/Vulkan.h"
#include "renderer/Swapchain.h"
#include "SceneLoader.h"
#include "Vertex.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include "renderer/renderer.h"
#include "renderer/AccelerationStructures.h"
#include "system/Input.h"
#include "Camera.h"
#include "Object.h"

struct InstanceMeshData
{
	uint32_t indexBufferOffset;
	uint32_t vertexBufferOffset;
	uint32_t materialIndex;
	int32_t meshIsLight;
	int32_t modelID;
};

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 1;

int RayTracing::raySampleCount = 1;
int RayTracing::rayDepth = 8;
bool RayTracing::showNormals = false;
bool RayTracing::showUniquePrimitives = false;
bool RayTracing::showAlbedo = false;
bool RayTracing::renderProgressive = false;

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

VkBuffer uniformBufferBuffer;
VkDeviceMemory uniformBufferMemory;

uint32_t facesCount;
uint32_t verticesSize;
uint32_t indicesSize;

struct UniformBuffer
{
	float cameraPosition[4] = { -1.433908, 3.579997, 5.812919, 1 };
	float cameraRight[4] = { 0.928479, 0, 0.371385, 1 };
	float cameraUp[4] = { 0, 1, 0, 1 };
	float cameraForward[4] = { 0.371385, 0, -0.928479, 1 };

	uint32_t frameCount = 0;
	int32_t showNormals = 0;
	int32_t showUnique = 0;
	int32_t showAlbedo = 0;
	int32_t raySamples = 2;
	int32_t rayDepth = 8;
	int32_t renderProgressive = 0;
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

void RayTracing::Destroy()
{
	vkFreeMemory(logicalDevice, uniformBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, uniformBufferBuffer, nullptr);

	TLAS->Destroy();
	for (BottomLevelAccelerationStructure* pBLAS : BLASs)
		pBLAS->Destroy();

	vkDestroyPipeline(logicalDevice, pipeline, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);

	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, materialSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);

	vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
}

void RayTracing::Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Win32Window* window, Swapchain* swapchain)
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

	creationObject = { logicalDevice, physicalDevice, queue, queueFamilyIndex };
	commandPool = Vulkan::FetchNewCommandPool(creationObject);

	// command buffer

	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

	result = vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, commandBuffers.data());
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to allocate the command buffers for ray tracing", result, nameof(vkAllocateCommandBuffers), __FILENAME__, std::to_string(__LINE__));

	TLAS = TopLevelAccelerationStructure::Create(creationObject, {}); // second parameter is empty since there are no models to build, not the best way to solve this

	// descriptor pool (frames in flight not implemented)

	std::vector<VkDescriptorPoolSize> descriptorPoolSizes(5);
	descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	descriptorPoolSizes[0].descriptorCount = 1;
	descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSizes[1].descriptorCount = 1;
	descriptorPoolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorPoolSizes[2].descriptorCount = 6;
	descriptorPoolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorPoolSizes[3].descriptorCount = 1;
	descriptorPoolSizes[4].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorPoolSizes[4].descriptorCount = Renderer::MAX_TLAS_INSTANCES;

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

	std::vector<VkDescriptorSetLayoutBinding> meshDataLayoutBindings(3);

	meshDataLayoutBindings[0].binding = 0;
	meshDataLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshDataLayoutBindings[0].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	meshDataLayoutBindings[0].descriptorCount = 1;
	meshDataLayoutBindings[0].pImmutableSamplers = nullptr;

	meshDataLayoutBindings[1].binding = 1;
	meshDataLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshDataLayoutBindings[1].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	meshDataLayoutBindings[1].descriptorCount = 1;
	meshDataLayoutBindings[1].pImmutableSamplers = nullptr;

	meshDataLayoutBindings[2].binding = 2;
	meshDataLayoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	meshDataLayoutBindings[2].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	meshDataLayoutBindings[2].descriptorCount = Renderer::MAX_BINDLESS_TEXTURES;
	meshDataLayoutBindings[2].pImmutableSamplers = nullptr;

	std::vector<VkDescriptorBindingFlags> bindingFlags;
	bindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	bindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	bindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);

	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagsCreateInfo{};
	bindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	bindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(meshDataLayoutBindings.size());
	bindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

	VkDescriptorSetLayoutCreateInfo meshDataLayoutCreateInfo{};
	meshDataLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	meshDataLayoutCreateInfo.bindingCount = static_cast<uint32_t>(meshDataLayoutBindings.size());
	meshDataLayoutCreateInfo.pBindings = meshDataLayoutBindings.data();
	meshDataLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	meshDataLayoutCreateInfo.pNext = &bindingFlagsCreateInfo;

	result = vkCreateDescriptorSetLayout(logicalDevice, &meshDataLayoutCreateInfo, nullptr, &materialSetLayout);
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

	VkShaderModule genShader = Vulkan::CreateShaderModule(logicalDevice, ReadShaderFile("shaders/spirv/gen.rgen.spv"));
	VkShaderModule hitShader = Vulkan::CreateShaderModule(logicalDevice, ReadShaderFile("shaders/spirv/hit.rchit.spv"));
	VkShaderModule missShader = Vulkan::CreateShaderModule(logicalDevice, ReadShaderFile("shaders/spirv/miss.rmiss.spv"));
	VkShaderModule shadowShader = Vulkan::CreateShaderModule(logicalDevice, ReadShaderFile("shaders/spirv/shadow.rmiss.spv"));

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

	// uniform buffer

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, sizeof(UniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, uniformBufferBuffer, uniformBufferMemory);

	vkMapMemory(logicalDevice, uniformBufferMemory, 0, sizeof(UniformBuffer), 0, &uniformBufferMemPtr);
	memcpy(uniformBufferMemPtr, &uniformBuffer, sizeof(UniformBuffer));

	CreateShaderBindingTable();

	VkWriteDescriptorSetAccelerationStructureKHR ASDescriptorInfo{};
	ASDescriptorInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	ASDescriptorInfo.accelerationStructureCount = 1;
	ASDescriptorInfo.pAccelerationStructures = &TLAS->accelerationStructure;

	VkDescriptorBufferInfo uniformDescriptorInfo{};
	uniformDescriptorInfo.buffer = uniformBufferBuffer;
	uniformDescriptorInfo.offset = 0;
	uniformDescriptorInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo indexDescriptorInfo{};
	indexDescriptorInfo.buffer = Renderer::globalIndicesBuffer.GetBufferHandle();
	indexDescriptorInfo.offset = 0;
	indexDescriptorInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo vertexDescriptorInfo{};
	vertexDescriptorInfo.buffer = Renderer::globalVertexBuffer.GetBufferHandle();
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

	CreateMeshDataBuffers();

	UpdateMeshDataDescriptorSets();

	CreateImage(swapchain->extent.width, swapchain->extent.height);
}

void RayTracing::CreateMeshDataBuffers()
{
	VkDeviceSize modelSize = sizeof(glm::mat4) * 8;// Renderer::MAX_MESHES;

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, modelSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, modelMatrixBuffer, modelMatrixBufferMemory);
	vkMapMemory(logicalDevice, modelMatrixBufferMemory, 0, modelSize, 0, &modelMatrixMemoryPointer);

	VkDeviceSize instanceDataSize = sizeof(InstanceMeshData) * 8;// Renderer::MAX_MESHES

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, instanceDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, instanceMeshDataBuffer, instanceMeshDataMemory);
	vkMapMemory(logicalDevice, instanceMeshDataMemory, 0, instanceDataSize, 0, &instanceMeshDataPointer);
}

void RayTracing::UpdateMeshDataDescriptorSets()
{
	VkDescriptorBufferInfo materialBufferDescriptorInfo{};
	materialBufferDescriptorInfo.buffer = materialBuffer;
	materialBufferDescriptorInfo.offset = 0;
	materialBufferDescriptorInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo modelMatrixBufferDescriptorInfo{};
	modelMatrixBufferDescriptorInfo.buffer = modelMatrixBuffer;
	modelMatrixBufferDescriptorInfo.offset = 0;
	modelMatrixBufferDescriptorInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo instanceDataBufferDescriptorInfo{};
	instanceDataBufferDescriptorInfo.buffer = instanceMeshDataBuffer;
	instanceDataBufferDescriptorInfo.offset = 0;
	instanceDataBufferDescriptorInfo.range = VK_WHOLE_SIZE;

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = VK_NULL_HANDLE;
	imageInfo.sampler = Renderer::defaultSampler;

	std::vector<VkDescriptorImageInfo> imageInfos(Renderer::MAX_BINDLESS_TEXTURES, imageInfo);

	std::vector<VkWriteDescriptorSet> meshDataWriteDescriptorSets(3);

	meshDataWriteDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	meshDataWriteDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshDataWriteDescriptorSets[0].dstSet = descriptorSets[1];
	meshDataWriteDescriptorSets[0].dstBinding = 0;
	meshDataWriteDescriptorSets[0].descriptorCount = 1;
	meshDataWriteDescriptorSets[0].pBufferInfo = &modelMatrixBufferDescriptorInfo;

	meshDataWriteDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	meshDataWriteDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshDataWriteDescriptorSets[1].dstSet = descriptorSets[1];
	meshDataWriteDescriptorSets[1].dstBinding = 1;
	meshDataWriteDescriptorSets[1].descriptorCount = 1;
	meshDataWriteDescriptorSets[1].pBufferInfo = &instanceDataBufferDescriptorInfo;

	meshDataWriteDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	meshDataWriteDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	meshDataWriteDescriptorSets[2].dstSet = descriptorSets[1];
	meshDataWriteDescriptorSets[2].dstBinding = 2;
	meshDataWriteDescriptorSets[2].descriptorCount = Renderer::MAX_BINDLESS_TEXTURES;
	meshDataWriteDescriptorSets[2].pImageInfo = imageInfos.data();

	vkUpdateDescriptorSets(logicalDevice, (uint32_t)meshDataWriteDescriptorSets.size(), meshDataWriteDescriptorSets.data(), 0, nullptr);
}

void RayTracing::CreateImage(uint32_t width, uint32_t height)
{
	if (RTImage != VK_NULL_HANDLE && RTImageMemory != VK_NULL_HANDLE && RTImageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(logicalDevice, RTImageView, nullptr);
		vkDestroyImage(logicalDevice, RTImage, nullptr);
		vkFreeMemory(logicalDevice, RTImageMemory, nullptr);
	}
	
	Vulkan::CreateImage(logicalDevice, physicalDevice, width, height, 1, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, RTImage, RTImageMemory);
	RTImageView = Vulkan::CreateImageView(logicalDevice, RTImage, VK_IMAGE_VIEW_TYPE_2D, 1, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	
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

	imageHasChanged = true;
}


void RayTracing::RecreateImage(Swapchain* swapchain)
{
	CreateImage(swapchain->extent.width, swapchain->extent.height);
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

	VkDeviceAddress shaderBindingTableBufferAddress = Vulkan::GetDeviceAddress(logicalDevice, shaderBindingTableBuffer);

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

void RayTracing::UpdateDescriptorSets()
{
	VkDescriptorImageInfo RTImageDescriptorImageInfo{};
	RTImageDescriptorImageInfo.sampler = VK_NULL_HANDLE;
	RTImageDescriptorImageInfo.imageView = RTImageView;
	RTImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	std::vector<VkWriteDescriptorSet> writeDescriptorSets;

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeSet.pImageInfo = &RTImageDescriptorImageInfo;
	writeSet.dstSet = descriptorSets[0];
	writeSet.descriptorCount = 1;
	writeSet.dstBinding = 4;
	writeDescriptorSets.push_back(writeSet);
	
	vkUpdateDescriptorSets(logicalDevice, (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

void RayTracing::UpdateTextureBuffer()
{
	uint32_t amountOfTexturesPerMaterial = rayTracingMaterialTextures.size();
	std::vector<VkDescriptorImageInfo> imageInfos(Mesh::materials.size() * amountOfTexturesPerMaterial);
	std::vector<VkWriteDescriptorSet> writeSets(Mesh::materials.size() * amountOfTexturesPerMaterial);
	for (int i = 0; i < Mesh::materials.size(); i++)
	{
		for (int j = 0; j < amountOfTexturesPerMaterial; j++)
		{
			uint32_t index = amountOfTexturesPerMaterial * i + j;
			
			VkDescriptorImageInfo& imageInfo = imageInfos[index];
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = Mesh::materials[i][rayTracingMaterialTextures[j]]->imageView;
			imageInfo.sampler = Renderer::defaultSampler;

			VkWriteDescriptorSet& writeSet = writeSets[index];
			writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeSet.pImageInfo = &imageInfos[index];
			writeSet.dstSet = descriptorSets[1];
			writeSet.descriptorCount = 1;
			writeSet.dstBinding = 2;
			writeSet.dstArrayElement = index;
		}
	}

	vkUpdateDescriptorSets(logicalDevice, (uint32_t)writeSets.size(), writeSets.data(), 0, nullptr);
}

void RayTracing::UpdateModelMatrices(const std::vector<Object*>& objects)
{
	std::vector<glm::mat4> modelMatrices;
	for (Object* object : objects)
	{
		if (object->state != STATUS_VISIBLE || !object->HasFinishedLoading())
			continue;

		glm::mat4 modelMatrix = object->transform.GetModelMatrix();
		for (Mesh& mesh : object->meshes)
			modelMatrices.push_back(modelMatrix);
	}
	memcpy(modelMatrixMemoryPointer, modelMatrices.data(), modelMatrices.size() * sizeof(glm::mat4));
}

void RayTracing::UpdateInstanceDataBuffer(const std::vector<Object*>& objects)
{
	std::vector<InstanceMeshData> instanceDatas;
	
	uint32_t indexOffset = 0;
	uint32_t vertexOffset = 0;

	for (int32_t i = 0; i < objects.size(); i++)
	{
		if (objects[i]->state != STATUS_VISIBLE || !objects[i]->HasFinishedLoading())
			continue;

		for (int32_t j = 0; j < objects[i]->meshes.size(); j++)
		{
			instanceDatas.push_back({ indexOffset, vertexOffset, objects[i]->meshes[j].materialIndex, 0, i * (int32_t)objects[i]->meshes.size() + j });
			
			indexOffset += objects[i]->meshes[j].indices.size();
			vertexOffset += objects[i]->meshes[j].vertices.size();
		}
	}
	instanceDatas[instanceDatas.size() - 1].meshIsLight = 1; // only last mesh is a light for testing
	memcpy(instanceMeshDataPointer, instanceDatas.data(), instanceDatas.size() * sizeof(InstanceMeshData));
}

uint32_t frameCount = 0;
void RayTracing::DrawFrame(std::vector<Object*> objects, Win32Window* window, Camera* camera, VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	if (imageHasChanged)
	{
		UpdateDescriptorSets();
		imageHasChanged = false;
	}

	UpdateModelMatrices(objects);
	UpdateInstanceDataBuffer(objects);

	if (Mesh::materials.size() > 0)
		UpdateTextureBuffer();
	
	int x, y;
	window->GetRelativeCursorPosition(x, y);
	if (x != 0 || y != 0 || Input::IsKeyPressed(VirtualKey::W) || Input::IsKeyPressed(VirtualKey::A) || Input::IsKeyPressed(VirtualKey::S) || Input::IsKeyPressed(VirtualKey::D) || showNormals)
		frameCount = 0;

	if (showNormals && showUniquePrimitives) showNormals = false; // can't 2 variables changing colors at once
	uniformBuffer = UniformBuffer{ { camera->position.x, camera->position.y, camera->position.z, 1 }, { camera->right.x, camera->right.y, camera->right.z, 1 }, { camera->up.x, camera->up.y, camera->up.z, 1 }, { camera->front.x, camera->front.y, camera->front.z, 1 }, frameCount, showNormals, showUniquePrimitives, showAlbedo, raySampleCount, rayDepth, renderProgressive };
	//uniformBuffer = UniformBuffer{ { 7.24205f, -4.13095f, 7.67253f, 1 }, { 0.70373f, 0.00000f, -0.71047f, 1 }, { -0.28477f, 0.91616f, -0.28206f, 1 }, { -0.65091f, -0.40081f, -0.64473f, 1 }, frameCount };
	//printf("pos: %.5f, %.5f, %.5f, right: %.5f, %.5f, %.5f, up: %.5f, %.5f, %.5f, front: %.5f, %.5f, %.5f\n", camera->position.x, camera->position.y, camera->position.z, camera->right.x, camera->right.y, camera->right.z, camera->up.x, camera->up.y, camera->up.z, camera->front.x, camera->front.y, camera->front.z);
	//std::cout << facesCount << std::endl;

	memcpy(uniformBufferMemPtr, &uniformBuffer, sizeof(UniformBuffer));

	TLAS->Build(creationObject, objects, false, commandBuffer);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	vkCmdTraceRaysKHR(commandBuffer, &rgenShaderBindingTable, &rmissShaderBindingTable, &rchitShaderBindingTable, &callableShaderBindingTable, swapchain->extent.width, swapchain->extent.height, 1);

	frameCount++;
}