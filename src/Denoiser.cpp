#include <stdexcept>
#include <iostream>
#include <string>
#include "renderer/Vulkan.h"
#include "renderer/Denoiser.h"

#include "glm.h"
#include "CreationObjects.h"

#include "optix_stubs.h"
#include "optix_function_table_definition.h"
#include "optix_function_table.h"

#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define CheckOptixResult(result) if (result != OPTIX_SUCCESS) { std::string message = (std::string)optixGetErrorString(result) + " at line " + std::to_string(__LINE__); throw std::runtime_error(message); }
#define CheckCudaResult(result) if (result != cudaSuccess) throw std::runtime_error(std::to_string(result) + " at line " + std::to_string(__LINE__) + " in " + (std::string)__FILENAME__);

void CallBack(unsigned int level, const char* tag, const char* message, void* cbdata)
{
	std::cout << message << "\n";
}

void Denoiser::InitOptix()
{
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
	denoiserOptions.guideNormal = 0;
	denoiserOptions.denoiseAlpha = OPTIX_DENOISER_ALPHA_MODE_COPY;

	optixResult = optixDenoiserCreate(optixContext, OPTIX_DENOISER_MODEL_KIND_LDR, &denoiserOptions, &denoiser); // not sure about the model kind
	CheckOptixResult(optixResult);

	CreateExternalSemaphore(externSemaphore, externSemaphoreHandle, cuExternSemaphore);

	//CreateExternalSemaphore(semaphore, semaphoreHandle, cuSemaphore);
	CheckCudaResult(cudaDeviceSynchronize());
	cudaError_t cuResult = cudaStreamSynchronize(cudaStream);
	CheckCudaResult(cuResult);
}

void Denoiser::AllocateBuffers(uint32_t width, uint32_t height)
{
	this->width = width;
	this->height = height;

	DestroyBuffers();

	OptixResult optixResult = optixDenoiserComputeMemoryResources(denoiser, width, height, &denoiserSizes);
	CheckOptixResult(optixResult);

	denoiserStateInBytes = denoiserSizes.stateSizeInBytes;
	CheckCudaResult(cudaMalloc((void**)&stateBuffer, denoiserSizes.stateSizeInBytes));
	CheckCudaResult(cudaMalloc((void**)&scratchBuffer, denoiserSizes.withoutOverlapScratchSizeInBytes));
	CheckCudaResult(cudaMalloc((void**)&minRGB, sizeof(glm::vec4)));

	optixResult = optixDenoiserSetup(denoiser, cudaStream, width, height, stateBuffer, denoiserSizes.stateSizeInBytes, scratchBuffer, denoiserSizes.withoutOverlapScratchSizeInBytes);
	CheckOptixResult(optixResult);

	CreateExternalCudaBuffer(denoiseCopyBuffer, denoiseCopyMemory, &cuDenoisecopyBuffer, copyHandle, (VkDeviceSize)width * height * sizeof(glm::vec4));
	CreateExternalCudaBuffer(albedoDenoiseBuffer, albedoDenoiseMemory, &cuAlbedoDenoise, albedoHandle, (VkDeviceSize)width * height * sizeof(glm::vec4));
	CreateExternalCudaBuffer(normalDenoiseBuffer, normalDenoiseMemory, &cuNormalDenoise, normalHandle, (VkDeviceSize)width * height * sizeof(glm::vec4));
	CreateExternalCudaBuffer(outputBuffer, outputMemory, &cuOutputBuffer, outputHandle, (VkDeviceSize)width * height * sizeof(glm::vec4));

	inputImage.data = (CUdeviceptr)cuDenoisecopyBuffer;
	inputImage.format = OPTIX_PIXEL_FORMAT_FLOAT4;
	inputImage.height = height;
	inputImage.width = width;
	inputImage.pixelStrideInBytes = sizeof(glm::vec4);
	inputImage.rowStrideInBytes = sizeof(glm::vec4) * width;

	outputImage.data = (CUdeviceptr)cuOutputBuffer;
	outputImage.format = OPTIX_PIXEL_FORMAT_FLOAT4;
	outputImage.height = height;
	outputImage.width = width;
	outputImage.pixelStrideInBytes = sizeof(glm::vec4);
	outputImage.rowStrideInBytes = sizeof(glm::vec4) * width;

	albedoImage.data = (CUdeviceptr)cuAlbedoDenoise;
	albedoImage.format = OPTIX_PIXEL_FORMAT_FLOAT4;
	albedoImage.height = height;
	albedoImage.width = width;
	albedoImage.pixelStrideInBytes = sizeof(glm::vec4);
	albedoImage.rowStrideInBytes = sizeof(glm::vec4) * width;

	normalImage.data = (CUdeviceptr)cuNormalDenoise;
	normalImage.format = OPTIX_PIXEL_FORMAT_FLOAT4;
	normalImage.height = height;
	normalImage.width = width;
	normalImage.pixelStrideInBytes = sizeof(glm::vec4);
	normalImage.rowStrideInBytes = sizeof(glm::vec4) * width;

	CheckCudaResult(cudaDeviceSynchronize());
	cudaError_t cuResult = cudaStreamSynchronize(cudaStream);
	CheckCudaResult(cuResult);
}

void Denoiser::CreateExternalCudaBuffer(VkBuffer& buffer, VkDeviceMemory& memory, void** cuPtr, HANDLE& handle, VkDeviceSize size)
{
	VkMemoryRequirements memReqs{};
	Vulkan::CreateExternalBuffer(logicalDevice, physicalDevice, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory);
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

void Denoiser::CreateExternalSemaphore(VkSemaphore& semaphore, HANDLE& handle, cudaExternalSemaphore_t& cuPtr)
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

void Denoiser::DenoiseImage()
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

	cudaExternalSemaphoreWaitParams waitParams{};
	waitParams.flags = 0;
	waitParams.params.fence.value = 0;
	cudaWaitExternalSemaphoresAsync(&cuExternSemaphore, &waitParams, 1, nullptr);

	OptixResult result = optixDenoiserInvoke(denoiser, cudaStream, &params, stateBuffer, denoiserSizes.stateSizeInBytes, &guideLayer, &layer, 1, 0, 0, scratchBuffer, denoiserSizes.withoutOverlapScratchSizeInBytes);
	CheckOptixResult(result);

	cudaExternalSemaphoreSignalParams signalParams{};
	signalParams.flags = 0;
	signalParams.params.fence.value = 2;
	cudaSignalExternalSemaphoresAsync(&cuExternSemaphore, &signalParams, 1, cudaStream);

	CheckCudaResult(cudaDeviceSynchronize());
	cuResult = cudaStreamSynchronize(cudaStream);
	CheckCudaResult(cuResult);
}

void Denoiser::CopyImagesToDenoisingBuffers(VkCommandBuffer commandBuffer, std::array<VkImage, 3> gBuffers)
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

	VkImageMemoryBarrier memoryBarriers2[] = { memoryBarrierInput, memoryBarrierAlbedo, memoryBarrierNormal };

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, std::size(memoryBarriers2), memoryBarriers2);
}

void Denoiser::CopyDenoisedBufferToImage(VkCommandBuffer commandBuffer, VkImage image)
{
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

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
	vkCmdCopyBufferToImage(commandBuffer, outputBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	memoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
}

void Denoiser::DestroyBuffers()
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

void Denoiser::Destroy()
{
	DestroyBuffers();
	optixDenoiserDestroy(denoiser);
	optixDeviceContextDestroy(optixContext);
}

Denoiser* Denoiser::Create(const VulkanCreationObject& creationObject)
{
	Denoiser* ret = new Denoiser();
	ret->logicalDevice = creationObject.logicalDevice;
	ret->physicalDevice = creationObject.physicalDevice;
	ret->InitOptix();
	return ret;
}