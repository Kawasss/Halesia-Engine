#define VK_USE_PLATFORM_WIN32_KHR
#include <fstream>
#include <vulkan/vulkan.h>
#include "renderer/RayTracing.h"
#include "renderer/Vulkan.h"
#include "renderer/Swapchain.h"
#include "system/Window.h"
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

#include "renderer/Denoiser.h"
#include "renderer/ShaderReflector.h"
#include "tools/common.h"

struct InstanceMeshData
{
	glm::mat4 transformation;
	uint32_t indexBufferOffset;
	uint32_t vertexBufferOffset;
	uint32_t materialIndex;
	int32_t meshIsLight;
	glm::vec2 motion;
	uint64_t intersectedObjectHandle;
};

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 1;

int RayTracing::raySampleCount = 1;
int RayTracing::rayDepth = 8;
bool RayTracing::showNormals = false;
bool RayTracing::showUniquePrimitives = false;
bool RayTracing::showAlbedo = false;
bool RayTracing::renderProgressive = false;
bool RayTracing::useWhiteAsAlbedo = false;
glm::vec3 RayTracing::directionalLightDir = glm::vec3(-0.5, -0.5, 0);

struct UniformBuffer
{
	glm::vec4 cameraPosition = glm::vec4(0);
	glm::mat4 viewInverse;
	glm::mat4 projectionInverse;
	glm::uvec2 mouseXY = glm::uvec2(0);

	uint32_t frameCount = 0;
	int32_t showUnique = 0;
	int32_t raySamples = 2;
	int32_t rayDepth = 8;
	int32_t renderProgressive = 0;
	int useWhiteAsAlbedo = 0;
	glm::vec3 directionalLightDir = glm::vec3(0, -1, 0);
};
UniformBuffer* uniformBufferMemPtr;

void RayTracing::Destroy()
{
	//vkDestroySemaphore(logicalDevice, semaphore, nullptr);
	//CloseHandle(semaphoreHandle);

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

RayTracing* RayTracing::Create(Win32Window* window, Swapchain* swapchain)
{
	RayTracing* ret = new RayTracing();
	ret->Init(window, swapchain);
	return ret;
}

void RayTracing::SetUp(Win32Window* window, Swapchain* swapchain)
{
	const Vulkan::Context& context = Vulkan::GetContext();
	this->logicalDevice = context.logicalDevice;
	this->physicalDevice = context.physicalDevice;
	this->swapchain = swapchain;
	this->window = window;

	commandPool = Vulkan::FetchNewCommandPool(context.graphicsIndex);

	VkPhysicalDeviceProperties2 properties2{};
	properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &rayTracingProperties;

	vkGetPhysicalDeviceProperties2(physicalDevice.Device(), &properties2);

	denoiser = Denoiser::Create();

	std::vector<Object*> holder = {};
	TLAS = TopLevelAccelerationStructure::Create(holder); // second parameter is empty since there are no models to build, not the best way to solve this
}

void RayTracing::CreateDescriptorPool() // (frames in flight not implemented)
{
	std::vector<VkDescriptorPoolSize> descriptorPoolSizes(5);
	descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	descriptorPoolSizes[0].descriptorCount = 1;
	descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSizes[1].descriptorCount = 1;
	descriptorPoolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorPoolSizes[2].descriptorCount = 6;
	descriptorPoolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorPoolSizes[3].descriptorCount = 3;
	descriptorPoolSizes[4].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorPoolSizes[4].descriptorCount = Renderer::MAX_TLAS_INSTANCES;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	descriptorPoolCreateInfo.maxSets = 2;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

	VkResult result = vkCreateDescriptorPool(logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool);
	CheckVulkanResult("Failed to create the descriptor pool for ray tracing", result, vkCreateDescriptorPool);
}

void RayTracing::CreateDescriptorSets(const std::vector<std::vector<char>> shaderCodes)
{
	ShaderGroupReflector groupReflection(shaderCodes);

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = groupReflection.GetLayoutBindingsOfSet(0);

	std::vector<VkDescriptorBindingFlags> setBindingFlags(setLayoutBindings.size(), VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setBindingFlagsCreateInfo{};
	setBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	setBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(setBindingFlags.size());
	setBindingFlagsCreateInfo.pBindingFlags = setBindingFlags.data();

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layoutCreateInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	layoutCreateInfo.pBindings = setLayoutBindings.data();
	layoutCreateInfo.pNext = &setBindingFlagsCreateInfo;

	VkResult result = vkCreateDescriptorSetLayout(logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
	CheckVulkanResult("Failed to create the descriptor set layout for ray tracing", result, vkCreateDescriptorSetLayout);

	std::vector<VkDescriptorSetLayoutBinding> meshDataLayoutBindings = groupReflection.GetLayoutBindingsOfSet(1);
	meshDataLayoutBindings[1].descriptorCount = Renderer::MAX_BINDLESS_TEXTURES;

	std::vector<VkDescriptorBindingFlags> bindingFlags(meshDataLayoutBindings.size(), VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagsCreateInfo{};
	bindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	bindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(meshDataLayoutBindings.size());
	bindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

	VkDescriptorSetLayoutCreateInfo meshDataLayoutCreateInfo{};
	meshDataLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	meshDataLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	meshDataLayoutCreateInfo.bindingCount = static_cast<uint32_t>(meshDataLayoutBindings.size());
	meshDataLayoutCreateInfo.pBindings = meshDataLayoutBindings.data();
	meshDataLayoutCreateInfo.pNext = &bindingFlagsCreateInfo;

	result = vkCreateDescriptorSetLayout(logicalDevice, &meshDataLayoutCreateInfo, nullptr, &materialSetLayout);
	CheckVulkanResult("Failed to create the material descriptor set layout for ray tracing", result, vkCreateDescriptorSetLayout);

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts{ descriptorSetLayout, materialSetLayout };

	VkDescriptorSetAllocateInfo setAllocateInfo{};
	setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocateInfo.descriptorPool = descriptorPool;
	setAllocateInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	setAllocateInfo.pSetLayouts = descriptorSetLayouts.data();

	descriptorSets.resize(descriptorSetLayouts.size());

	result = vkAllocateDescriptorSets(logicalDevice, &setAllocateInfo, descriptorSets.data());
	CheckVulkanResult("Failed to allocate the descriptor sets for ray tracing", result, vkAllocateDescriptorSets);
}

void RayTracing::CreateRayTracingPipeline(const std::vector<std::vector<char>> shaderCodes) // 0 is rgen code, 1 is chit code, 2 is rmiss code, 3 is shadow code
{
	VkShaderModule genShader = Vulkan::CreateShaderModule(shaderCodes[0]);
	VkShaderModule hitShader = Vulkan::CreateShaderModule(shaderCodes[1]);
	VkShaderModule missShader = Vulkan::CreateShaderModule(shaderCodes[2]);
	VkShaderModule shadowShader = Vulkan::CreateShaderModule(shaderCodes[3]);

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts{ descriptorSetLayout, materialSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VkResult result = vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	CheckVulkanResult("Failed to create the pipeline layout for ray tracing", result, vkCreatePipelineLayout);

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
	CheckVulkanResult("Failed to create the pipeline for ray tracing", result, vkCreateRayTracingPipelinesKHR);

	vkDestroyShaderModule(logicalDevice, shadowShader, nullptr);
	vkDestroyShaderModule(logicalDevice, missShader, nullptr);
	vkDestroyShaderModule(logicalDevice, hitShader, nullptr);
	vkDestroyShaderModule(logicalDevice, genShader, nullptr);
}

void RayTracing::CreateBuffers()
{
	Vulkan::CreateBuffer(sizeof(UniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, uniformBufferBuffer, uniformBufferMemory);

	vkMapMemory(logicalDevice, uniformBufferMemory, 0, sizeof(UniformBuffer), 0, (void**)&uniformBufferMemPtr);

	VkDeviceSize instanceDataSize = sizeof(InstanceMeshData) * Renderer::MAX_MESHES;

	Vulkan::CreateBuffer(instanceDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, instanceMeshDataBuffer, instanceMeshDataMemory);
	vkMapMemory(logicalDevice, instanceMeshDataMemory, 0, instanceDataSize, 0, (void**)&instanceMeshDataPointer);

	CreateShaderBindingTable();

	Vulkan::CreateBuffer(sizeof(uint64_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, handleBuffer, handleBufferMemory);
	vkMapMemory(logicalDevice, handleBufferMemory, 0, sizeof(uint64_t), 0, &handleBufferMemPointer);

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

	VkDescriptorBufferInfo handleBufferInfo{};
	handleBufferInfo.buffer = handleBuffer;
	handleBufferInfo.offset = 0;
	handleBufferInfo.range = VK_WHOLE_SIZE;

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
	writeDescriptorSets[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeDescriptorSets[4].pNext = &ASDescriptorInfo;
	writeDescriptorSets[4].dstSet = descriptorSets[0];
	writeDescriptorSets[4].descriptorCount = 1;
	writeDescriptorSets[4].dstBinding = 7;
	writeDescriptorSets[4].pBufferInfo = &handleBufferInfo;

	vkUpdateDescriptorSets(logicalDevice, (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
	UpdateMeshDataDescriptorSets();
}

void RayTracing::Init(Win32Window* window, Swapchain* swapchain)
{
	const std::vector<char> rgenCode = ReadFile("shaders/spirv/gen.rgen.spv"), chitCode = ReadFile("shaders/spirv/hit.rchit.spv");
	const std::vector<char> rmissCode = ReadFile("shaders/spirv/miss.rmiss.spv"), shadowCode = ReadFile("shaders/spirv/shadow.rmiss.spv");

	SetUp(window, swapchain);
	CreateDescriptorPool();
	CreateDescriptorSets({ rgenCode, chitCode, rmissCode, shadowCode });
	CreateRayTracingPipeline({ rgenCode, chitCode, rmissCode, shadowCode });
	CreateBuffers();
	CreateImage(swapchain->extent.width, swapchain->extent.height);
}

void RayTracing::UpdateMeshDataDescriptorSets()
{
	VkDescriptorBufferInfo materialBufferDescriptorInfo{};
	materialBufferDescriptorInfo.buffer = materialBuffer;
	materialBufferDescriptorInfo.offset = 0;
	materialBufferDescriptorInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo instanceDataBufferDescriptorInfo{};
	instanceDataBufferDescriptorInfo.buffer = instanceMeshDataBuffer;
	instanceDataBufferDescriptorInfo.offset = 0;
	instanceDataBufferDescriptorInfo.range = VK_WHOLE_SIZE;

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = VK_NULL_HANDLE;
	imageInfo.sampler = Renderer::defaultSampler;

	std::vector<VkDescriptorImageInfo> imageInfos(Renderer::MAX_BINDLESS_TEXTURES, imageInfo);

	std::vector<VkWriteDescriptorSet> meshDataWriteDescriptorSets(2);

	meshDataWriteDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	meshDataWriteDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshDataWriteDescriptorSets[0].dstSet = descriptorSets[1];
	meshDataWriteDescriptorSets[0].dstBinding = 0;
	meshDataWriteDescriptorSets[0].descriptorCount = 1;
	meshDataWriteDescriptorSets[0].pBufferInfo = &instanceDataBufferDescriptorInfo;

	meshDataWriteDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	meshDataWriteDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	meshDataWriteDescriptorSets[1].dstSet = descriptorSets[1];
	meshDataWriteDescriptorSets[1].dstBinding = 1;
	meshDataWriteDescriptorSets[1].descriptorCount = Renderer::MAX_BINDLESS_TEXTURES;
	meshDataWriteDescriptorSets[1].pImageInfo = imageInfos.data();

	vkUpdateDescriptorSets(logicalDevice, (uint32_t)meshDataWriteDescriptorSets.size(), meshDataWriteDescriptorSets.data(), 0, nullptr);
}

void RayTracing::CreateImage(uint32_t width, uint32_t height)
{
	this->width = width;
	this->height = height;
	const Vulkan::Context& context = Vulkan::GetContext();
	if (gBuffers[0] != VK_NULL_HANDLE)
	{
		for (int i = 0; i < gBuffers.size(); i++)
		{
			vkDestroyImageView(logicalDevice, gBufferViews[i], nullptr);
			vkDestroyImage(logicalDevice, gBuffers[i], nullptr);
			vkFreeMemory(logicalDevice, gBufferMemories[i], nullptr);
		}
	}
	
	for (int i = 0; i < gBuffers.size(); i++)
	{
		Vulkan::CreateImage(width, height, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, gBuffers[i], gBufferMemories[i]);
		gBufferViews[i] = Vulkan::CreateImageView(gBuffers[i], VK_IMAGE_VIEW_TYPE_2D, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);

		VkImageMemoryBarrier RTImageMemoryBarrier{};
		RTImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		RTImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		RTImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		RTImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		RTImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		RTImageMemoryBarrier.image = gBuffers[i];
		RTImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		RTImageMemoryBarrier.subresourceRange.levelCount = 1;
		RTImageMemoryBarrier.subresourceRange.layerCount = 1;

		VkCommandBuffer imageBarrierCommandBuffer = Vulkan::BeginSingleTimeCommands(commandPool);
		vkCmdPipelineBarrier(imageBarrierCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &RTImageMemoryBarrier);
		Vulkan::EndSingleTimeCommands(context.graphicsQueue, imageBarrierCommandBuffer, commandPool);
	}
	imageHasChanged = true;
	denoiser->AllocateBuffers(width, height);
}


void RayTracing::RecreateImage(Win32Window* window)
{
	CreateImage(window->GetWidth(), window->GetHeight());
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

	Vulkan::CreateBuffer(shaderBindingTableSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, shaderBindingTableBuffer, shaderBindingTableMemory);

	std::vector<char> shaderBuffer(shaderBindingTableSize);
	VkResult result = vkGetRayTracingShaderGroupHandlesKHR(logicalDevice, pipeline, 0, 4, shaderBindingTableSize, shaderBuffer.data());
	CheckVulkanResult("Failed to get the ray tracing shader group handles", result, vkGetRayTracingShaderGroupHandlesKHR);

	void* shaderBindingTableMemPtr;
	result = vkMapMemory(logicalDevice, shaderBindingTableMemory, 0, shaderBindingTableSize, 0, &shaderBindingTableMemPtr);
	CheckVulkanResult("Failed to map the shader binding table memory", result, vkMapMemory);

	for (uint32_t i = 0; i < 4; i++) // 4 = amount of shaders
	{
		memcpy(shaderBindingTableMemPtr, shaderBuffer.data() + i * rayTracingProperties.shaderGroupHandleSize, rayTracingProperties.shaderGroupHandleSize);
		shaderBindingTableMemPtr = static_cast<char*>(shaderBindingTableMemPtr) + rayTracingProperties.shaderGroupBaseAlignment;
	}
	vkUnmapMemory(logicalDevice, shaderBindingTableMemory);

	VkDeviceAddress shaderBindingTableBufferAddress = Vulkan::GetDeviceAddress(shaderBindingTableBuffer);

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
	RTImageDescriptorImageInfo.imageView = gBufferViews[0];
	RTImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkDescriptorImageInfo albedoImageDescriptorImageInfo{};
	albedoImageDescriptorImageInfo.sampler = VK_NULL_HANDLE;
	albedoImageDescriptorImageInfo.imageView = gBufferViews[1];
	albedoImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkDescriptorImageInfo normalImageDescriptorImageInfo{};
	normalImageDescriptorImageInfo.sampler = VK_NULL_HANDLE;
	normalImageDescriptorImageInfo.imageView = gBufferViews[2];
	normalImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	std::array<VkWriteDescriptorSet, 3> writeSets{};

	writeSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeSets[0].pImageInfo = &RTImageDescriptorImageInfo;
	writeSets[0].dstSet = descriptorSets[0];
	writeSets[0].descriptorCount = 1;
	writeSets[0].dstBinding = 4;

	writeSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeSets[1].pImageInfo = &albedoImageDescriptorImageInfo;
	writeSets[1].dstSet = descriptorSets[0];
	writeSets[1].descriptorCount = 1;
	writeSets[1].dstBinding = 5;

	writeSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSets[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeSets[2].pImageInfo = &normalImageDescriptorImageInfo;
	writeSets[2].dstSet = descriptorSets[0];
	writeSets[2].descriptorCount = 1;
	writeSets[2].dstBinding = 6;
	
	vkUpdateDescriptorSets(logicalDevice, (uint32_t)writeSets.size(), writeSets.data(), 0, nullptr);
}

void RayTracing::UpdateTextureBuffer()
{
	uint32_t amountOfTexturesPerMaterial = rayTracingMaterialTextures.size();
	std::vector<VkDescriptorImageInfo> imageInfos(Mesh::materials.size() * amountOfTexturesPerMaterial);
	std::vector<VkWriteDescriptorSet> writeSets;
	for (int i = 0; i < Mesh::materials.size(); i++)
	{
		if (processedMaterials.count(i) > 0 && processedMaterials[i] == Mesh::materials[i].handle)
			continue;

		for (int j = 0; j < amountOfTexturesPerMaterial; j++)
		{
			uint32_t index = amountOfTexturesPerMaterial * i + j;

			VkDescriptorImageInfo& imageInfo = imageInfos[index];
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = Mesh::materials[i][rayTracingMaterialTextures[j]]->imageView;
			imageInfo.sampler = Renderer::defaultSampler;

			VkWriteDescriptorSet writeSet{};
			writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeSet.pImageInfo = &imageInfos[index];
			writeSet.dstSet = descriptorSets[1];
			writeSet.descriptorCount = 1;
			writeSet.dstBinding = 1;
			writeSet.dstArrayElement = index;
			writeSets.push_back(writeSet);
		}
		processedMaterials[i] = Mesh::materials[i].handle;
	}
	if (!writeSets.empty())
		vkUpdateDescriptorSets(logicalDevice, (uint32_t)writeSets.size(), writeSets.data(), 0, nullptr);
}

void RayTracing::UpdateInstanceDataBuffer(const std::vector<Object*>& objects, Camera* camera)
{
	amountOfActiveObjects = 0;
	for (int32_t i = 0; i < objects.size(); i++, amountOfActiveObjects++)
	{
		if (objects[i]->state != OBJECT_STATE_VISIBLE || !objects[i]->HasFinishedLoading())
			continue;
		
		std::lock_guard<std::mutex> lockGuard(objects[i]->mutex);

		glm::vec2 ndc = objects[i]->transform.GetMotionVector(camera->GetProjectionMatrix(), camera->GetViewMatrix());
		ndc.x *= width;
		ndc.y *= height;
		
		for (int32_t j = 0; j < objects[i]->meshes.size(); j++)
		{
			Mesh& mesh = objects[i]->meshes[j];
			instanceMeshDataPointer[i * objects[i]->meshes.size() + j] =
			{ 
				objects[i]->transform.GetModelMatrix(), 
				(uint32_t)Renderer::globalIndicesBuffer.GetItemOffset(mesh.indexMemory), 
				(uint32_t)Renderer::globalVertexBuffer.GetItemOffset(mesh.vertexMemory), 
				mesh.materialIndex, 
				Mesh::materials[mesh.materialIndex].isLight, 
				ndc,
				objects[i]->handle
			};
		}
	}
}

uint32_t frameCount = 0;
void RayTracing::DrawFrame(std::vector<Object*> objects, Win32Window* window, Camera* camera, uint32_t width, uint32_t height, VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	if (imageHasChanged)
	{
		UpdateDescriptorSets();
		imageHasChanged = false;
	}
	if (!TLAS->HasBeenBuilt() && !objects.empty())
		TLAS->Build(objects);

	UpdateInstanceDataBuffer(objects, camera);

	if (amountOfActiveObjects <= 0)
		return;

	if (!Mesh::materials.empty())
		UpdateTextureBuffer();

	int x, y, absX, absY;
	window->GetRelativeCursorPosition(x, y);
	window->GetAbsoluteCursorPosition(absX, absY);
	
	/*if (x != 0 || y != 0 || Input::IsKeyPressed(VirtualKey::W) || Input::IsKeyPressed(VirtualKey::A) || Input::IsKeyPressed(VirtualKey::S) || Input::IsKeyPressed(VirtualKey::D) || showNormals)
		frameCount = 0;*/
	
	if (showNormals && showUniquePrimitives) showNormals = false; // can't have 2 variables changing colors at once
	*uniformBufferMemPtr = UniformBuffer{ { camera->position, 1 }, glm::inverse(camera->GetViewMatrix()), glm::inverse(camera->GetProjectionMatrix()), glm::uvec2((uint32_t)absX, (uint32_t)absY), frameCount, showUniquePrimitives, raySampleCount, rayDepth, renderProgressive, 0, directionalLightDir};

	TLAS->Update(objects, commandBuffer);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	vkCmdTraceRaysKHR(commandBuffer, &rgenShaderBindingTable, &rmissShaderBindingTable, &rchitShaderBindingTable, &callableShaderBindingTable, width, height, 1);
	frameCount++;
}

void RayTracing::DenoiseImage()
{
	denoiser->DenoiseImage();
}

void RayTracing::ApplyDenoisedImage(VkCommandBuffer commandBuffer)
{
	denoiser->CopyDenoisedBufferToImage(commandBuffer, gBuffers[0]);
}

void RayTracing::PrepareForDenoising(VkCommandBuffer commandBuffer)
{
	denoiser->CopyImagesToDenoisingBuffers(commandBuffer, gBuffers);
}