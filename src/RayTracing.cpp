#include <fstream>
#include <vulkan/vulkan.h>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "renderer/renderer.h"
#include "renderer/AccelerationStructures.h"
#include "renderer/Denoiser.h"
#include "renderer/ShaderReflector.h"
#include "renderer/Vertex.h"
#include "renderer/RayTracing.h"
#include "renderer/Vulkan.h"

#include "system/Input.h"
#include "system/Window.h"

#include "core/Camera.h"
#include "core/Object.h"

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
	glm::vec2 cameraMotion;
	glm::vec3 directionalLightDir = glm::vec3(0, -1, 0);
};
UniformBuffer* uniformBufferMemPtr;

void RayTracing::Destroy()
{
	//vkDestroySemaphore(logicalDevice, semaphore, nullptr);
	//CloseHandle(semaphoreHandle);

	/*vkFreeMemory(logicalDevice, uniformBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, uniformBufferBuffer, nullptr);*/

	delete TLAS;

	vkDestroyPipeline(logicalDevice, pipeline, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);

	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, materialSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);

	for (int i = 0; i < gBuffers.size(); i++)
	{
		vkDestroyImageView(logicalDevice, gBufferViews[i], nullptr);
		vkDestroyImage(logicalDevice, gBuffers[i], nullptr);
		vkFreeMemory(logicalDevice, gBufferMemories[i], nullptr);
	}
	vkDestroyImageView(logicalDevice, prevImageView, nullptr);
	vkDestroyImage(logicalDevice, prevImage, nullptr);
	vkFreeMemory(logicalDevice, prevMemory, nullptr);

	vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
}

RayTracing* RayTracing::Create(Window* window)
{
	RayTracing* ret = new RayTracing();
	ret->Init(window);
	return ret;
}

void RayTracing::SetUp(Window* window)
{
	const Vulkan::Context& context = Vulkan::GetContext();
	this->logicalDevice = context.logicalDevice;
	this->window = window;

	commandPool = Vulkan::FetchNewCommandPool(context.graphicsIndex);

	VkPhysicalDeviceProperties2 properties2{};
	properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &rayTracingProperties;

	vkGetPhysicalDeviceProperties2(context.physicalDevice.Device(), &properties2);

	denoiser = Denoiser::Create();

	std::vector<Object*> holder = {};
	TLAS = TopLevelAccelerationStructure::Create(holder); // second parameter is empty since there are no models to build, not the best way to solve this
}

void RayTracing::CreateDescriptorPool(const ShaderGroupReflector& groupReflection) // (frames in flight not implemented)
{
	std::vector<VkDescriptorPoolSize> descriptorPoolSizes = groupReflection.GetDescriptorPoolSize();
	for (int i = 0; i < descriptorPoolSizes.size(); i++)
		if (descriptorPoolSizes[i].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)		
			descriptorPoolSizes[i].descriptorCount = Renderer::MAX_TLAS_INSTANCES; // reflection cannot detect the size of the texture array in this shader, so its size has to be overwritten (maybe change it into a parameter for the function?)

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	descriptorPoolCreateInfo.maxSets = 2;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

	VkResult result = vkCreateDescriptorPool(logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool);
	CheckVulkanResult("Failed to create the descriptor pool for ray tracing", result, vkCreateDescriptorPool);
}

void RayTracing::CreateDescriptorSets(const ShaderGroupReflector& groupReflection)
{
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

void RayTracing::CreateBuffers(const ShaderGroupReflector& groupReflection)
{
	uniformBufferBuffer.Init(sizeof(UniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	uniformBufferMemPtr = uniformBufferBuffer.Map<UniformBuffer>();

	VkDeviceSize instanceDataSize = sizeof(InstanceMeshData) * Renderer::MAX_MESHES;
	instanceMeshDataBuffer.Init(instanceDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	instanceMeshDataPointer = instanceMeshDataBuffer.Map<InstanceMeshData>();

	handleBuffer.Init(sizeof(uint64_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	handleBufferMemPointer = handleBuffer.Map<uint64_t>();

	CreateShaderBindingTable();

	VkWriteDescriptorSetAccelerationStructureKHR ASDescriptorInfo{};
	ASDescriptorInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	ASDescriptorInfo.accelerationStructureCount = 1;
	ASDescriptorInfo.pAccelerationStructures = &TLAS->accelerationStructure;

	VkWriteDescriptorSet writeSet{};

	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	writeSet.pNext = &ASDescriptorInfo;
	writeSet.dstSet = descriptorSets[0];
	writeSet.descriptorCount = 1;
	writeSet.dstBinding = 0;

	groupReflection.WriteToDescriptorSet(logicalDevice, descriptorSets[0], uniformBufferBuffer.Get(), 0, 1);
	groupReflection.WriteToDescriptorSet(logicalDevice, descriptorSets[0], Renderer::g_indexBuffer.GetBufferHandle(), 0, 2);
	groupReflection.WriteToDescriptorSet(logicalDevice, descriptorSets[0], Renderer::g_vertexBuffer.GetBufferHandle(), 0, 3);
	groupReflection.WriteToDescriptorSet(logicalDevice, descriptorSets[0], handleBuffer.Get(), 0, 7);
	groupReflection.WriteToDescriptorSet(logicalDevice, descriptorSets[0], denoiser->GetMotionBuffer(), 0, 8);

	vkUpdateDescriptorSets(logicalDevice, 1, &writeSet, 0, nullptr);
	UpdateMeshDataDescriptorSets();
}

void RayTracing::CreateMotionBuffer()
{
	VkDeviceSize size = width * height * sizeof(glm::vec2);
	motionBuffer.Init(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void RayTracing::Init(Window* window)
{
	const std::vector<char> rgenCode = ReadFile("shaders/spirv/gen.rgen.spv"), chitCode = ReadFile("shaders/spirv/hit.rchit.spv");
	const std::vector<char> rmissCode = ReadFile("shaders/spirv/miss.rmiss.spv"), shadowCode = ReadFile("shaders/spirv/shadow.rmiss.spv");
	ShaderGroupReflector groupReflection({ rgenCode, chitCode, rmissCode, shadowCode });

	SetUp(window);
	CreateDescriptorPool(groupReflection);
	CreateDescriptorSets(groupReflection);
	CreateRayTracingPipeline({ rgenCode, chitCode, rmissCode, shadowCode });
	CreateBuffers(groupReflection);
	//CreateImage(swapchain->extent.width, swapchain->extent.height);
}

void RayTracing::UpdateMeshDataDescriptorSets()
{
	VkDescriptorBufferInfo materialBufferDescriptorInfo{};
	materialBufferDescriptorInfo.buffer = materialBuffer.Get();
	materialBufferDescriptorInfo.offset = 0;
	materialBufferDescriptorInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo instanceDataBufferDescriptorInfo{};
	instanceDataBufferDescriptorInfo.buffer = instanceMeshDataBuffer.Get();
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
		vkDestroyImageView(logicalDevice, prevImageView, nullptr);
		vkDestroyImage(logicalDevice, prevImage, nullptr);
		vkFreeMemory(logicalDevice, prevMemory, nullptr);

		motionBuffer.Destroy();
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
	Vulkan::CreateImage(width, height, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, prevImage, prevMemory);
	prevImageView = Vulkan::CreateImageView(prevImage, VK_IMAGE_VIEW_TYPE_2D, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);

	CreateMotionBuffer();

	imageHasChanged = true;
	if (Renderer::denoiseOutput)
		denoiser->AllocateBuffers(width, height);
}


void RayTracing::RecreateImage(uint32_t width, uint32_t height)
{
	CreateImage(width, height);
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

inline VkDescriptorImageInfo GetImageInfo(VkImageView imageView)
{
	VkDescriptorImageInfo ret{};
	ret.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	ret.sampler = VK_NULL_HANDLE;
	ret.imageView = imageView;
	return ret;
}

inline VkWriteDescriptorSet WriteSetImage(VkDescriptorSet dstSet, uint32_t dstBinding, VkDescriptorImageInfo* pImageInfo)
{
	VkWriteDescriptorSet ret{};
	ret.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	ret.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	ret.pImageInfo = pImageInfo;
	ret.dstSet = dstSet;
	ret.descriptorCount = 1;
	ret.dstBinding = dstBinding;
	return ret;
}

void RayTracing::CopyPreviousResult(VkCommandBuffer commandBuffer)
{
	VkImageSubresourceRange subresourceRange{};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;

	VkImageMemoryBarrier barriers[2]{};
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = gBuffers[0];
	barriers[0].subresourceRange = subresourceRange;

	barriers[1] = barriers[0];
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].image = prevImage;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

	VkImageCopy imageCopy{};
	imageCopy.extent = { width, height, 1 };
	imageCopy.dstSubresource.layerCount = 1;
	imageCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopy.srcSubresource.layerCount = 1;
	imageCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	vkCmdCopyImage(commandBuffer, gBuffers[0], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, prevImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

	barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);
}

void RayTracing::UpdateDescriptorSets()
{
	VkDescriptorImageInfo RTImageDescriptorImageInfo = GetImageInfo(gBufferViews[0]);
	VkDescriptorImageInfo albedoImageDescriptorImageInfo = GetImageInfo(gBufferViews[1]);
	VkDescriptorImageInfo normalImageDescriptorImageInfo = GetImageInfo(gBufferViews[2]);
	VkDescriptorImageInfo prevImageDescriptorImageInfo = GetImageInfo(prevImageView);

	VkDescriptorBufferInfo motionBufferInfo{};
	motionBufferInfo.buffer = motionBuffer.Get();
	motionBufferInfo.offset = 0;
	motionBufferInfo.range = VK_WHOLE_SIZE;

	std::array<VkWriteDescriptorSet, 5> writeSets{};
	writeSets[0] = WriteSetImage(descriptorSets[0], 4, &RTImageDescriptorImageInfo);
	writeSets[1] = WriteSetImage(descriptorSets[0], 5, &albedoImageDescriptorImageInfo);
	writeSets[2] = WriteSetImage(descriptorSets[0], 6, &normalImageDescriptorImageInfo);
	writeSets[3] = WriteSetImage(descriptorSets[0], 9, &prevImageDescriptorImageInfo);
	writeSets[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSets[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeSets[4].dstSet = descriptorSets[0];
	writeSets[4].descriptorCount = 1;
	writeSets[4].dstBinding = 8;
	writeSets[4].pBufferInfo = &motionBufferInfo;
	
	vkUpdateDescriptorSets(logicalDevice, (uint32_t)writeSets.size(), writeSets.data(), 0, nullptr);
}

void RayTracing::UpdateTextureBuffer()
{
	uint32_t amountOfTexturesPerMaterial = rayTracingMaterialTextures.size();
	std::vector<VkDescriptorImageInfo> imageInfos(Mesh::materials.size() * amountOfTexturesPerMaterial);
	std::vector<VkWriteDescriptorSet> writeSets;
	for (int i = 0; i < Mesh::materials.size(); i++)
	{
		if ((processedMaterials.count(i) > 0 && processedMaterials[i] == Mesh::materials[i].handle) || !Mesh::materials[i].HasFinishedLoading())
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
	amountOfActiveObjects = 1;
	glm::vec2 staticMotion = camera->GetMotionVector() * glm::vec2(width, height); // this only factors in the rotation of the camera, not the changes in position

	for (int32_t i = 0; i < objects.size(); i++, amountOfActiveObjects++)
	{
		glm::vec2 ndc = objects[i]->rigid.type == RIGID_BODY_DYNAMIC ? objects[i]->transform.GetMotionVector(camera->GetProjectionMatrix(), camera->GetViewMatrix()) * glm::vec2(width, height) : staticMotion;
		Mesh& mesh = objects[i]->mesh;
		instanceMeshDataPointer[i] =
		{ 
			objects[i]->transform.GetModelMatrix(), 
			(uint32_t)Renderer::g_indexBuffer.GetItemOffset(mesh.indexMemory), 
			(uint32_t)Renderer::g_vertexBuffer.GetItemOffset(mesh.vertexMemory), 
			mesh.materialIndex, 
			Mesh::materials[mesh.materialIndex].isLight, 
			ndc,
			objects[i]->handle
		};
	}
}

uint32_t frameCount = 0;
void RayTracing::DrawFrame(std::vector<Object*> objects, Window* window, Camera* camera, VkCommandBuffer commandBuffer)
{
	if (gBuffers[0] == VK_NULL_HANDLE)
		RecreateImage(window->GetWidth(), window->GetHeight());

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

	if (Input::IsKeyPressed(VirtualKey::R)) // r for reset
		frameCount = 0;
	
	if (showNormals && showUniquePrimitives) showNormals = false; // can't have 2 variables changing colors at once
	uniformBufferMemPtr->cameraPosition = { camera->position, 1 };
	uniformBufferMemPtr->viewInverse = glm::inverse(camera->GetViewMatrix());
	uniformBufferMemPtr->projectionInverse = glm::inverse(camera->GetProjectionMatrix());
	uniformBufferMemPtr->mouseXY = glm::uvec2((uint32_t)(absX * Renderer::internalScale), (uint32_t)(absY * Renderer::internalScale));
	uniformBufferMemPtr->frameCount = frameCount;
	uniformBufferMemPtr->showUnique = showUniquePrimitives;
	uniformBufferMemPtr->raySamples = raySampleCount;
	uniformBufferMemPtr->rayDepth = rayDepth;
	uniformBufferMemPtr->renderProgressive = renderProgressive;
	uniformBufferMemPtr->directionalLightDir = directionalLightDir;

	TLAS->Update(objects, commandBuffer);
	CopyPreviousResult(commandBuffer);
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