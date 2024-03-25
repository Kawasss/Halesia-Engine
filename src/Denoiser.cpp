#include <stdexcept>
#include <iostream>
#include <string>
#include "renderer/Vulkan.h"
#include "renderer/Denoiser.h"

#include "glm.h"

#include "optix_stubs.h"
#include "optix_function_table_definition.h"
#include "optix_function_table.h"

#ifdef USE_CUDA
#pragma comment(lib, "cudart_static.lib")
#endif

#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define CheckOptixResult(result) if (result != OPTIX_SUCCESS) { std::string message = (std::string)optixGetErrorString(result) + " at line " + std::to_string(__LINE__); throw std::runtime_error(message); }
#define CheckCudaResult(result) if (result != cudaSuccess) throw std::runtime_error(std::to_string(result) + " at line " + std::to_string(__LINE__) + " in " + (std::string)__FILENAME__);

void CallBack(unsigned int level, const char* tag, const char* message, void* cbdata)
{
	std::cout << message << "\n";
}

void Denoiser::InitOptix()
{
#ifdef USE_CUDA
	cudaError_t error = cudaFree(nullptr); // init runtime cuda
	cudaContext = nullptr;
	CheckCudaResult(error);
	cudaStreamCreate(&cudaStream);
	OptixResult optixResult = optixInit();
	CheckOptixResult(optixResult);

	OptixDeviceContextOptions optixDeviceOptions{};
	optixDeviceOptions.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_ALL;
	optixDeviceOptions.logCallbackFunction = &CallBack;
	optixDeviceOptions.logCallbackLevel = 4;
	optixResult = optixDeviceContextCreate(cudaContext, &optixDeviceOptions, &optixContext);
	CheckOptixResult(optixResult);

	OptixDenoiserOptions denoiserOptions{};
	denoiserOptions.guideAlbedo = 1;
	denoiserOptions.guideNormal = 1;
	denoiserOptions.denoiseAlpha = OPTIX_DENOISER_ALPHA_MODE_COPY;
	
	optixResult = optixDenoiserCreate(optixContext, OPTIX_DENOISER_MODEL_KIND_TEMPORAL, &denoiserOptions, &denoiser); // not sure about the model kind
	CheckOptixResult(optixResult);

	CreateExternalSemaphore(externSemaphore, externSemaphoreHandle, cuExternSemaphore);

	//CreateExternalSemaphore(semaphore, semaphoreHandle, cuSemaphore);
	CheckCudaResult(cudaDeviceSynchronize());
	cudaError_t cuResult = cudaStreamSynchronize(cudaStream);
	CheckCudaResult(cuResult);
#endif
}

void Denoiser::AllocateBuffers(uint32_t width, uint32_t height)
{
#ifdef USE_CUDA
	this->width = width;
	this->height = height;

	DestroyBuffers();

	OptixResult optixResult = optixDenoiserComputeMemoryResources(denoiser, width, height, &denoiserSizes);
	CheckOptixResult(optixResult);

	denoiserStateInBytes = denoiserSizes.stateSizeInBytes;
	CheckCudaResult(cudaMalloc((void**)&stateBuffer, denoiserSizes.stateSizeInBytes));
	CheckCudaResult(cudaMalloc((void**)&scratchBuffer, denoiserSizes.withoutOverlapScratchSizeInBytes));
	CheckCudaResult(cudaMalloc((void**)&minRGB, sizeof(glm::vec4)));
	glm::vec4 test = glm::vec4(1);
	cudaMemcpy((void*)minRGB, &test, sizeof(test), cudaMemcpyDefault);

	optixResult = optixDenoiserSetup(denoiser, cudaStream, width, height, stateBuffer, denoiserSizes.stateSizeInBytes, scratchBuffer, denoiserSizes.withoutOverlapScratchSizeInBytes);
	CheckOptixResult(optixResult);

	input.Create(width, height, sizeof(glm::vec4), this);
	output.Create(width, height, sizeof(glm::vec4), this);
	albedo.Create(width, height, sizeof(glm::vec4), this);
	normal.Create(width, height, sizeof(glm::vec4), this);
	motion.Create(width, height, sizeof(glm::vec2), this);

	CheckCudaResult(cudaDeviceSynchronize());
	cudaError_t cuResult = cudaStreamSynchronize(cudaStream);
	CheckCudaResult(cuResult);
#endif
}

void Denoiser::CreateExternalCudaBuffer(VkBuffer& buffer, VkDeviceMemory& memory, void** cuPtr, HANDLE& handle, VkDeviceSize size)
{
#ifdef USE_CUDA
	const Vulkan::Context& context = Vulkan::GetContext();

	VkMemoryRequirements memReqs{};
	Vulkan::CreateExternalBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory);
	vkGetBufferMemoryRequirements(context.logicalDevice, buffer, &memReqs);

	handle = Vulkan::GetWin32MemoryHandle(memory);

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
#endif
}

void Denoiser::CreateExternalSemaphore(VkSemaphore& semaphore, HANDLE& handle, cudaExternalSemaphore_t& cuPtr)
{
#ifdef USE_CUDA
	const Vulkan::Context& context = Vulkan::GetContext();

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

	VkResult vkResult = vkCreateSemaphore(context.logicalDevice, &semaphoreCreateInfo, nullptr, &semaphore);
	CheckVulkanResult("Failed to create a semaphore", vkResult, vkCreateSemaphore);

	VkSemaphoreGetWin32HandleInfoKHR getHandleInfo{};
	getHandleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
	getHandleInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	getHandleInfo.semaphore = semaphore;

	vkResult = vkGetSemaphoreWin32HandleKHR(context.logicalDevice, &getHandleInfo, &handle);
	CheckVulkanResult("Failed to get the win32 handle of a semaphore", vkResult, vkGetSemaphoreWin32HandleKHR);

	cudaExternalSemaphoreHandleDesc externSemaphoreDesc{};
	externSemaphoreDesc.type = cudaExternalSemaphoreHandleTypeTimelineSemaphoreWin32;
	externSemaphoreDesc.handle.win32.handle = handle;
	externSemaphoreDesc.flags = 0;

	CheckCudaResult(cudaImportExternalSemaphore(&cuPtr, &externSemaphoreDesc));
#endif
}

void Denoiser::DenoiseImage()
{
#ifdef USE_CUDA
	CheckCudaResult(cudaDeviceSynchronize());
	cudaError_t cuResult = cudaStreamSynchronize(cudaStream);
	CheckCudaResult(cuResult);

	OptixDenoiserParams params{};
	params.blendFactor = 0;
	params.hdrAverageColor = minRGB;
	params.hdrIntensity = minRGB;
	params.temporalModeUsePreviousLayers = 0;

	OptixDenoiserLayer layer{};
	layer.input = input.image;
	layer.output = output.image;
	layer.previousOutput = output.image;

	OptixDenoiserGuideLayer guideLayer{};
	guideLayer.albedo = albedo.image;
	guideLayer.normal = normal.image;
	guideLayer.flow = motion.image;
	guideLayer.flowTrustworthiness.data = 0;
	guideLayer.outputInternalGuideLayer.data = 0;
	guideLayer.previousOutputInternalGuideLayer.data = 0;

	OptixResult result = optixDenoiserInvoke(denoiser, cudaStream, &params, stateBuffer, denoiserSizes.stateSizeInBytes, &guideLayer, &layer, 1, 0, 0, scratchBuffer, denoiserSizes.withoutOverlapScratchSizeInBytes);
	CheckOptixResult(result);

	CheckCudaResult(cudaDeviceSynchronize());
	cuResult = cudaStreamSynchronize(cudaStream);
	CheckCudaResult(cuResult);
#endif
}

void Denoiser::CopyImagesToDenoisingBuffers(VkCommandBuffer commandBuffer, std::array<VkImage, 3> gBuffers)
{
#ifdef USE_CUDA
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
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, (uint32_t)std::size(memoryBarriers), memoryBarriers);

	VkBufferImageCopy imageCopy{};
	imageCopy.imageExtent = { width, height, 1 };
	imageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopy.imageSubresource.baseArrayLayer = 0;
	imageCopy.imageSubresource.mipLevel = 0;
	imageCopy.imageSubresource.layerCount = 1;

	vkCmdCopyImageToBuffer(commandBuffer, gBuffers[0], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, input.vkBuffer, 1, &imageCopy);
	vkCmdCopyImageToBuffer(commandBuffer, gBuffers[1], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, albedo.vkBuffer, 1, &imageCopy);
	vkCmdCopyImageToBuffer(commandBuffer, gBuffers[2], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, normal.vkBuffer, 1, &imageCopy);

	memoryBarrierInput.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	memoryBarrierInput.newLayout = VK_IMAGE_LAYOUT_GENERAL;

	memoryBarrierAlbedo.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	memoryBarrierAlbedo.newLayout = VK_IMAGE_LAYOUT_GENERAL;

	memoryBarrierNormal.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	memoryBarrierNormal.newLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkImageMemoryBarrier memoryBarriers2[] = { memoryBarrierInput, memoryBarrierAlbedo, memoryBarrierNormal };
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, (uint32_t)std::size(memoryBarriers2), memoryBarriers2);
#endif
}

void Denoiser::CopyDenoisedBufferToImage(VkCommandBuffer commandBuffer, VkImage image)
{
#ifdef USE_CUDA
	VkImageSubresourceRange subresourceRange{};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;

	VkImageMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	memoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.image = image;
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

	VkBufferCopy copyRegion{};
	copyRegion.size = (VkDeviceSize)width * height * sizeof(glm::vec4);

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
	vkCmdCopyBufferToImage(commandBuffer, output.vkBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	memoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
#endif
}

void Denoiser::SharedOptixImage::Create(uint32_t width, uint32_t height, size_t pixelSize, Denoiser* denoiser)
{
#ifdef USE_CUDA
	denoiser->CreateExternalCudaBuffer(vkBuffer, vkMemory, &cudaBuffer, winHandle, (VkDeviceSize)width * height * pixelSize);

	image.data = (CUdeviceptr)cudaBuffer;
	image.format = pixelSize == sizeof(glm::vec4) ? OPTIX_PIXEL_FORMAT_FLOAT4 : OPTIX_PIXEL_FORMAT_FLOAT2;
	image.height = height;
	image.width = width;
	image.pixelStrideInBytes = pixelSize;
	image.rowStrideInBytes = pixelSize * width;
#endif
}

void Denoiser::SharedOptixImage::Destroy(VkDevice logicalDevice)
{
#ifdef USE_CUDA
	if (winHandle != (void*)0)
		CloseHandle(winHandle);
	if (vkBuffer != VK_NULL_HANDLE)
		vkDestroyBuffer(logicalDevice, vkBuffer, nullptr);
	if (vkMemory != VK_NULL_HANDLE)
		vkFreeMemory(logicalDevice, vkMemory, nullptr);
	if (cudaBuffer != 0)
		CheckCudaResult(cudaFree(cudaBuffer));
#endif
}

void Denoiser::DestroyBuffers()
{
#ifdef USE_CUDA
	if (stateBuffer != 0)
		cudaFree((void*)stateBuffer);
	if (scratchBuffer != 0)
		cudaFree((void*)scratchBuffer);
	if (minRGB != 0)
		cudaFree((void*)minRGB);

	const Vulkan::Context& context = Vulkan::GetContext();
	input.Destroy(context.logicalDevice);
	output.Destroy(context.logicalDevice);
	albedo.Destroy(context.logicalDevice);
	normal.Destroy(context.logicalDevice);
	motion.Destroy(context.logicalDevice);
#endif
}

void Denoiser::Destroy()
{
	DestroyBuffers();
	optixDenoiserDestroy(denoiser);
	optixDeviceContextDestroy(optixContext);
}

Denoiser* Denoiser::Create()
{
	Denoiser* ret = new Denoiser();
	ret->InitOptix();
	return ret;
}