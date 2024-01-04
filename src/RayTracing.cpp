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

#include "optix_stubs.h"
#include "optix_function_table_definition.h"
#include "optix_function_table.h"
#include "cuda_runtime_api.h"

#define CheckOptixResult(result) if (result != OPTIX_SUCCESS) { std::string message = (std::string)optixGetErrorString(result) + " at line " + std::to_string(__LINE__); throw std::runtime_error(message); }
#define CheckCudaResult(result) if (result != cudaSuccess) throw std::runtime_error(std::to_string(result) + " at line " + std::to_string(__LINE__) + " in " + (std::string)__FILENAME__);

struct InstanceMeshData
{
	glm::mat4 transformation;
	uint32_t indexBufferOffset;
	uint32_t vertexBufferOffset;
	uint32_t materialIndex;
	int32_t meshIsLight;
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
	DestroyOptixBuffers();

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

void CallBack(unsigned int level, const char* tag, const char* message, void* cbdata)
{
	std::cout << message << "\n";

}

void RayTracing::InitOptix()
{
	//cudaError_t error = cudaFree(nullptr); // init runtime cuda
	//cudaContext = nullptr;
	//CheckCudaResult(error);
	//cudaStreamCreate(&cudaStream);
	//OptixResult optixResult = optixInit();
	//CheckOptixResult(optixResult);
	//
	//OptixDeviceContextOptions optixDeviceOptions{};
	//optixDeviceOptions.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_ALL;
	//optixDeviceOptions.logCallbackFunction = &CallBack;
	//optixDeviceOptions.logCallbackLevel = 4;
	//optixResult = optixDeviceContextCreate(cudaContext, &optixDeviceOptions, &optixContext);
	//CheckOptixResult(optixResult);

	//OptixDenoiserOptions denoiserOptions{};
	//denoiserOptions.guideAlbedo = 0;
	//denoiserOptions.guideNormal = 0;
	//denoiserOptions.denoiseAlpha = OPTIX_DENOISER_ALPHA_MODE_DENOISE;

	//optixResult = optixDenoiserCreate(optixContext, OPTIX_DENOISER_MODEL_KIND_LDR, &denoiserOptions, &denoiser); // not sure about the model kind
	//CheckOptixResult(optixResult);

	////CreateExternalSemaphore(semaphore, semaphoreHandle, cuSemaphore);
	//CheckCudaResult(cudaDeviceSynchronize());
	//cudaError_t cuResult = cudaStreamSynchronize(cudaStream);
	//CheckCudaResult(cuResult);
}	

void RayTracing::CopyImagesToDenoisingBuffers(VkCommandBuffer commandBuffer)
{
	VkImageSubresourceRange subresourceRange{};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;

	VkImageMemoryBarrier memoryBarrierInput{};
	memoryBarrierInput.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrierInput.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	memoryBarrierInput.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	memoryBarrierInput.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrierInput.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrierInput.image = gBuffers[0];
	memoryBarrierInput.subresourceRange = subresourceRange;

	VkImageMemoryBarrier memoryBarrierAlbedo = memoryBarrierInput;
	memoryBarrierAlbedo.image = gBuffers[1];

	VkImageMemoryBarrier memoryBarrierNormal = memoryBarrierInput;
	memoryBarrierNormal.image = gBuffers[2];
	VkImageMemoryBarrier memoryBarriers[] = { memoryBarrierInput, memoryBarrierAlbedo, memoryBarrierNormal };

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, std::size(memoryBarriers), memoryBarriers);

	VkBufferImageCopy imageCopy{};
	imageCopy.imageExtent = { width, height, 1 };
	imageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopy.imageSubresource.baseArrayLayer = 0;
	imageCopy.imageSubresource.mipLevel = 0;
	imageCopy.imageSubresource.layerCount = 1;

	vkCmdCopyImageToBuffer(commandBuffer, gBuffers[0], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, denoiseCopyBuffer, 1, &imageCopy);
	vkCmdCopyImageToBuffer(commandBuffer, gBuffers[1], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, albedoDenoiseBuffer, 1, &imageCopy);
	vkCmdCopyImageToBuffer(commandBuffer, gBuffers[2], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, normalDenoiseBuffer, 1, &imageCopy);

	memoryBarrierInput.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	memoryBarrierInput.newLayout = VK_IMAGE_LAYOUT_GENERAL;

	memoryBarrierAlbedo.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	memoryBarrierAlbedo.newLayout = VK_IMAGE_LAYOUT_GENERAL;

	memoryBarrierNormal.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	memoryBarrierNormal.newLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkImageMemoryBarrier memoryBarriers2[] = {memoryBarrierInput, memoryBarrierAlbedo, memoryBarrierNormal};

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, std::size(memoryBarriers2), memoryBarriers2);
}

void RayTracing::AllocateOptixBuffers(uint32_t width, uint32_t height)
{
	/*DestroyOptixBuffers();

	OptixResult optixResult = optixDenoiserComputeMemoryResources(denoiser, width, height, &denoiserSizes);
	CheckOptixResult(optixResult);
	
	denoiserStateInBytes = denoiserSizes.stateSizeInBytes;
	CheckCudaResult(cudaMalloc((void**)&stateBuffer, denoiserSizes.stateSizeInBytes));
	CheckCudaResult(cudaMalloc((void**)&scratchBuffer, denoiserSizes.withoutOverlapScratchSizeInBytes));
	CheckCudaResult(cudaMalloc((void**)&minRGB, sizeof(glm::vec4)));

	optixResult = optixDenoiserSetup(denoiser, cudaStream, width, height, stateBuffer, denoiserSizes.stateSizeInBytes, scratchBuffer, denoiserSizes.withoutOverlapScratchSizeInBytes);
	CheckOptixResult(optixResult);
	
	CreateExternalCudaBuffer(denoiseCopyBuffer, denoiseCopyMemory, &cuDenoisecopyBuffer, copyHandle, (VkDeviceSize)width * height * 4);
	CreateExternalCudaBuffer(albedoDenoiseBuffer, albedoDenoiseMemory, &cuAlbedoDenoise, albedoHandle, (VkDeviceSize)width * height * 4);
	CreateExternalCudaBuffer(normalDenoiseBuffer, normalDenoiseMemory, &cuNormalDenoise, normalHandle, (VkDeviceSize)width * height * 4);
	CreateExternalCudaBuffer(outputBuffer, outputMemory, &cuOutputBuffer, outputHandle, (VkDeviceSize)width * height * 4);

	inputImage.data = (CUdeviceptr)cuDenoisecopyBuffer;
	inputImage.format = OPTIX_PIXEL_FORMAT_FLOAT4;
	inputImage.height = height;
	inputImage.width = width;
	inputImage.pixelStrideInBytes = sizeof(glm::vec4);
	inputImage.rowStrideInBytes = sizeof(glm::vec4)* width;

	outputImage.data = (CUdeviceptr)cuOutputBuffer;
	outputImage.format = OPTIX_PIXEL_FORMAT_FLOAT4;
	outputImage.height = height;
	outputImage.width = width;
	outputImage.pixelStrideInBytes = sizeof(glm::vec4);
	outputImage.rowStrideInBytes = sizeof(glm::vec4)* width;

	albedoImage.data = (CUdeviceptr)cuAlbedoDenoise;
	albedoImage.format = OPTIX_PIXEL_FORMAT_FLOAT4;
	albedoImage.height = height;
	albedoImage.width = width;
	albedoImage.pixelStrideInBytes = sizeof(glm::vec4);
	albedoImage.rowStrideInBytes = sizeof(glm::vec4)* width;

	normalImage.data = (CUdeviceptr)cuNormalDenoise;
	normalImage.format = OPTIX_PIXEL_FORMAT_FLOAT4;
	normalImage.height = height;
	normalImage.width = width;
	normalImage.pixelStrideInBytes = sizeof(glm::vec4);
	normalImage.rowStrideInBytes = sizeof(glm::vec4)* width;

	CheckCudaResult(cudaDeviceSynchronize());
	cudaError_t cuResult = cudaStreamSynchronize(cudaStream);
	CheckCudaResult(cuResult);*/
}

void RayTracing::CreateExternalCudaBuffer(VkBuffer& buffer, VkDeviceMemory& memory, void** cuPtr, HANDLE& handle, VkDeviceSize size)
{
	VkMemoryRequirements memReqs{};
	Vulkan::CreateExternalBuffer(logicalDevice, physicalDevice, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory);
	vkGetBufferMemoryRequirements(logicalDevice, buffer, &memReqs);

	handle = Vulkan::GetWin32MemoryHandle(logicalDevice, memory);

	cudaExternalMemory_t extMemory{}; // import the vk buffer to a cuda buffer
	cudaExternalMemoryHandleDesc memoryDesc{};
	memoryDesc.type = cudaExternalMemoryHandleTypeOpaqueWin32;
	memoryDesc.handle.win32.handle = handle;
	memoryDesc.size = memReqs.size;
	memoryDesc.flags = cudaExternalMemoryDedicated;

	cudaError_t cuResult = cudaImportExternalMemory(&extMemory, &memoryDesc);
	CheckCudaResult(cuResult);

	cudaExternalMemoryBufferDesc bufferDesc{};
	bufferDesc.size = memReqs.size;
	bufferDesc.offset = 0;
	bufferDesc.flags = 0;

	cuResult = cudaExternalMemoryGetMappedBuffer(cuPtr, extMemory, &bufferDesc);
	CheckCudaResult(cuResult);
}

void RayTracing::CreateExternalSemaphore(VkSemaphore& semaphore, HANDLE& handle, cudaExternalSemaphore_t& cuPtr)
{
	VkSemaphoreTypeCreateInfo semaphoreTypeInfo{};
	semaphoreTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	semaphoreTypeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	semaphoreTypeInfo.initialValue = 0;

	VkExportSemaphoreCreateInfo semaphoreExportInfo{};
	semaphoreExportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
	semaphoreExportInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	semaphoreExportInfo.pNext = &semaphoreTypeInfo;

	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = &semaphoreExportInfo;

	VkResult vkResult = vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &semaphore);
	CheckVulkanResult("Failed to create a semaphore", vkResult, vkCreateSemaphore);

	VkSemaphoreGetWin32HandleInfoKHR getHandleInfo{};
	getHandleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
	getHandleInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	getHandleInfo.semaphore = semaphore;

	vkResult = vkGetSemaphoreWin32HandleKHR(logicalDevice, &getHandleInfo, &handle);
	CheckVulkanResult("Failed to get the win32 handle of a semaphore", vkResult, vkGetSemaphoreWin32HandleKHR);

	cudaExternalSemaphoreHandleDesc externSemaphoreDesc{};
	externSemaphoreDesc.type = cudaExternalSemaphoreHandleTypeTimelineSemaphoreWin32;
	externSemaphoreDesc.handle.win32.handle = handle;
	externSemaphoreDesc.flags = 0;

	CheckCudaResult(cudaImportExternalSemaphore(&cuPtr, &externSemaphoreDesc));
}

void RayTracing::DenoiseImage(VkCommandBuffer commandBuffer)
{
	CheckCudaResult(cudaDeviceSynchronize());
	cudaError_t cuResult = cudaStreamSynchronize(cudaStream);
	CheckCudaResult(cuResult);

	OptixDenoiserParams params{};
	params.blendFactor = 0;
	params.hdrAverageColor = 0;
	params.hdrIntensity = 0;
	params.temporalModeUsePreviousLayers = 0;

	OptixDenoiserLayer layer{};
	layer.input = inputImage;
	layer.output = outputImage;
	layer.previousOutput.data = 0;

	OptixDenoiserGuideLayer guideLayer{};
	guideLayer.albedo = albedoImage;
	guideLayer.normal = normalImage;
	guideLayer.flow.data = 0;
	guideLayer.flowTrustworthiness.data = 0;
	guideLayer.outputInternalGuideLayer.data = 0;
	guideLayer.previousOutputInternalGuideLayer.data = 0;
	
	OptixResult result = optixDenoiserInvoke(denoiser, cudaStream, &params, stateBuffer, denoiserSizes.stateSizeInBytes, &guideLayer, &layer, 1, 0, 0, scratchBuffer, denoiserSizes.withoutOverlapScratchSizeInBytes);
	CheckOptixResult(result);

	CheckCudaResult(cudaDeviceSynchronize());
	cuResult = cudaStreamSynchronize(cudaStream);
	CheckCudaResult(cuResult);
}

void RayTracing::CopyDenoisedBufferToImage(VkCommandBuffer commandBuffer)
{
	VkImageSubresourceRange subresourceRange{};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;

	VkImageMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	memoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.image = gBuffers[0];
	memoryBarrier.subresourceRange = subresourceRange;

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
	vkCmdCopyBufferToImage(commandBuffer, outputBuffer, gBuffers[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	memoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
}

void RayTracing::DestroyOptixBuffers()
{
	if (stateBuffer != 0)
		cudaFree((void*)stateBuffer);
	if (scratchBuffer != 0)
		cudaFree((void*)scratchBuffer);
	if (minRGB != 0)
		cudaFree((void*)minRGB);

	if (copyHandle != (void*)0)
		CloseHandle(copyHandle);
	if (denoiseCopyBuffer != VK_NULL_HANDLE)
		vkDestroyBuffer(logicalDevice, denoiseCopyBuffer, nullptr);
	if (denoiseCopyMemory != VK_NULL_HANDLE)
		vkFreeMemory(logicalDevice, denoiseCopyMemory, nullptr);

	if (albedoHandle != (void*)0)
		CloseHandle(albedoHandle);
	if (albedoDenoiseBuffer != VK_NULL_HANDLE)
		vkDestroyBuffer(logicalDevice, albedoDenoiseBuffer, nullptr);
	if (albedoDenoiseMemory != VK_NULL_HANDLE)
		vkFreeMemory(logicalDevice, albedoDenoiseMemory, nullptr);

	if (normalHandle != (void*)0)
		CloseHandle(normalHandle);
	if (normalDenoiseBuffer != VK_NULL_HANDLE)
		vkDestroyBuffer(logicalDevice, normalDenoiseBuffer, nullptr);
	if (normalDenoiseMemory != VK_NULL_HANDLE)
		vkFreeMemory(logicalDevice, normalDenoiseMemory, nullptr);
}

void RayTracing::Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Win32Window* window, Swapchain* swapchain)
{
	VkResult result = VK_SUCCESS;
	
	this->logicalDevice = logicalDevice;
	this->physicalDevice = physicalDevice;
	this->swapchain = swapchain;
	this->window = window;

	InitOptix();

	rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

	VkPhysicalDeviceProperties2 properties2{};
	properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &rayTracingProperties;

	vkGetPhysicalDeviceProperties2(physicalDevice.Device(), &properties2);

	if (!physicalDevice.QueueFamilies(surface).graphicsFamily.has_value())
		throw VulkanAPIError("No appropriate graphics family could be found for ray tracing", VK_SUCCESS, nameof(physicalDevice.QueueFamilies(surface).graphicsFamily.has_value()), __FILENAME__, __LINE__);

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
	CheckVulkanResult("Failed to allocate the command buffers for ray tracing", result, vkAllocateCommandBuffers);

	std::vector<Object*> holder = {};
	TLAS = TopLevelAccelerationStructure::Create(creationObject, holder); // second parameter is empty since there are no models to build, not the best way to solve this

	// descriptor pool (frames in flight not implemented)

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

	result = vkCreateDescriptorPool(logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool);
	CheckVulkanResult("Failed to create the descriptor pool for ray tracing", result, vkCreateDescriptorPool);

	// descriptor set layout (frames in flight not implemented)

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings(8);

	setLayoutBindings[0].binding = 0;
	setLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	setLayoutBindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	setLayoutBindings[0].descriptorCount = 1;
	setLayoutBindings[0].pImmutableSamplers = nullptr;

	setLayoutBindings[1].binding = 1;
	setLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	setLayoutBindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR  | VK_SHADER_STAGE_MISS_BIT_KHR;
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

	setLayoutBindings[5].binding = 5;
	setLayoutBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	setLayoutBindings[5].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	setLayoutBindings[5].descriptorCount = 1;
	setLayoutBindings[5].pImmutableSamplers = nullptr;

	setLayoutBindings[6].binding = 6;
	setLayoutBindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	setLayoutBindings[6].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	setLayoutBindings[6].descriptorCount = 1;
	setLayoutBindings[6].pImmutableSamplers = nullptr;

	setLayoutBindings[7].binding = 7;
	setLayoutBindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	setLayoutBindings[7].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	setLayoutBindings[7].descriptorCount = 1;
	setLayoutBindings[7].pImmutableSamplers = nullptr;

	std::vector<VkDescriptorBindingFlags> setBindingFlags;
	for (int i = 0; i < setLayoutBindings.size(); i++)
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
	CheckVulkanResult("Failed to create the descriptor set layout for ray tracing", result, vkCreateDescriptorSetLayout);

	std::vector<VkDescriptorSetLayoutBinding> meshDataLayoutBindings(2);

	meshDataLayoutBindings[0].binding = 0;
	meshDataLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshDataLayoutBindings[0].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	meshDataLayoutBindings[0].descriptorCount = 1;
	meshDataLayoutBindings[0].pImmutableSamplers = nullptr;

	meshDataLayoutBindings[1].binding = 1;
	meshDataLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	meshDataLayoutBindings[1].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	meshDataLayoutBindings[1].descriptorCount = Renderer::MAX_BINDLESS_TEXTURES;
	meshDataLayoutBindings[1].pImmutableSamplers = nullptr;

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
	CheckVulkanResult("Failed to create the material descriptor set layout for ray tracing", result, vkCreateDescriptorSetLayout);

	// allocate the descriptor sets

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts{ descriptorSetLayout, materialSetLayout };

	VkDescriptorSetAllocateInfo setAllocateInfo{};
	setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocateInfo.descriptorPool = descriptorPool;
	setAllocateInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	setAllocateInfo.pSetLayouts = descriptorSetLayouts.data();

	descriptorSets.resize(descriptorSetLayouts.size());

	result = vkAllocateDescriptorSets(logicalDevice, &setAllocateInfo, descriptorSets.data());
	CheckVulkanResult("Failed to allocate the descriptor sets for ray tracing", result, vkAllocateDescriptorSets);

	// pipeline layout

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	result = vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	CheckVulkanResult("Failed to create the pipeline layout for ray tracing", result, vkCreatePipelineLayout);

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
	CheckVulkanResult("Failed to create the pipeline for ray tracing", result, vkCreateRayTracingPipelinesKHR);

	vkDestroyShaderModule(logicalDevice, shadowShader, nullptr);
	vkDestroyShaderModule(logicalDevice, missShader, nullptr);
	vkDestroyShaderModule(logicalDevice, hitShader, nullptr);
	vkDestroyShaderModule(logicalDevice, genShader, nullptr);

	// uniform buffer

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, sizeof(UniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, uniformBufferBuffer, uniformBufferMemory);

	vkMapMemory(logicalDevice, uniformBufferMemory, 0, sizeof(UniformBuffer), 0, &uniformBufferMemPtr);
	memcpy(uniformBufferMemPtr, &uniformBuffer, sizeof(UniformBuffer));

	CreateShaderBindingTable();

	// handle write buffer

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, sizeof(uint64_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, handleBuffer, handleBufferMemory);
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

	CreateMeshDataBuffers();

	UpdateMeshDataDescriptorSets();

	CreateImage(swapchain->extent.width, swapchain->extent.height);
}

void RayTracing::CreateMeshDataBuffers()
{
	VkDeviceSize instanceDataSize = sizeof(InstanceMeshData) * Renderer::MAX_MESHES;

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, instanceDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, instanceMeshDataBuffer, instanceMeshDataMemory);
	vkMapMemory(logicalDevice, instanceMeshDataMemory, 0, instanceDataSize, 0, &instanceMeshDataPointer);
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
	if (gBuffers[0] != VK_NULL_HANDLE)
	{
		for (int i = 0; i < 3; i++)
		{
			vkDestroyImageView(logicalDevice, gBufferViews[i], nullptr);
			vkDestroyImage(logicalDevice, gBuffers[i], nullptr);
			vkFreeMemory(logicalDevice, gBufferMemories[i], nullptr);
		}
	}
	
	for (int i = 0; i < 3; i++)
	{
		Vulkan::CreateImage(logicalDevice, physicalDevice, width, height, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, gBuffers[i], gBufferMemories[i]);
		gBufferViews[i] = Vulkan::CreateImageView(logicalDevice, gBuffers[i], VK_IMAGE_VIEW_TYPE_2D, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

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

		VkCommandBuffer imageBarrierCommandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);
		vkCmdPipelineBarrier(imageBarrierCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &RTImageMemoryBarrier);
		Vulkan::EndSingleTimeCommands(logicalDevice, queue, imageBarrierCommandBuffer, commandPool);
	}
	imageHasChanged = true;
	//AllocateOptixBuffers(width, height);
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

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, shaderBindingTableSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, shaderBindingTableBuffer, shaderBindingTableMemory);

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

void RayTracing::UpdateInstanceDataBuffer(const std::vector<Object*>& objects)
{
	std::vector<InstanceMeshData> instanceDatas;

	amountOfActiveObjects = 0;
	for (int32_t i = 0; i < objects.size(); i++, amountOfActiveObjects++)
	{
		if (objects[i]->state != OBJECT_STATE_VISIBLE || !objects[i]->HasFinishedLoading())
			continue;
		
		std::lock_guard<std::mutex> lockGuard(objects[i]->mutex);
		for (int32_t j = 0; j < objects[i]->meshes.size(); j++)
		{
			Mesh& mesh = objects[i]->meshes[j];
			instanceDatas.push_back({ objects[i]->transform.GetModelMatrix(), (uint32_t)Renderer::globalIndicesBuffer.GetItemOffset(mesh.indexMemory), (uint32_t)Renderer::globalVertexBuffer.GetItemOffset(mesh.vertexMemory), mesh.materialIndex, Mesh::materials[mesh.materialIndex].isLight, objects[i]->handle});
		}
	}
	if (instanceDatas.empty())
		return;
	
	memcpy(instanceMeshDataPointer, instanceDatas.data(), instanceDatas.size() * sizeof(InstanceMeshData));
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
		TLAS->Build(creationObject, objects);

	UpdateInstanceDataBuffer(objects);

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
	uniformBuffer = UniformBuffer{ { camera->position, 1 }, glm::inverse(camera->GetViewMatrix()), glm::inverse(camera->GetProjectionMatrix()), glm::uvec2((uint32_t)absX, (uint32_t)absY), frameCount, showUniquePrimitives, raySampleCount, rayDepth, renderProgressive, 0, directionalLightDir};
	
	memcpy(uniformBufferMemPtr, &uniformBuffer, sizeof(UniformBuffer));
	TLAS->Update(creationObject, objects, commandBuffer);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	vkCmdTraceRaysKHR(commandBuffer, &rgenShaderBindingTable, &rmissShaderBindingTable, &rchitShaderBindingTable, &callableShaderBindingTable, width, height, 1);
	//CopyImagesToDenoisingBuffers(commandBuffer);
	frameCount++;
}