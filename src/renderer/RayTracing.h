#pragma once
#include "renderer/PhysicalDevice.h"
#include "../Object.h"
#include "../Camera.h"
#include "Texture.h"
#include "Swapchain.h"

struct BottomLevelAccelerationStructure
{
	VkBuffer buffer;
	VkDeviceMemory deviceMemory;
	VkAccelerationStructureKHR accelerationStructure;

	VkBuffer scratchBuffer;
	VkDeviceMemory scratchDeviceMemory;

	VkBuffer geometryInstanceBuffer;
	VkDeviceMemory geometryInstanceBufferMemory;

	VkDeviceAddress deviceAddress;
};

struct TopLevelAccelerationStructure
{
	VkBuffer buffer;
	VkDeviceMemory deviceMemory;
	VkAccelerationStructureKHR accelerationStructure;

	VkBuffer scratchBuffer;
	VkDeviceMemory scratchMemory;
};

class RayTracing
{
public:
	void Destroy(VkDevice logicalDevice);
	void Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Object* object, Camera* camera, Win32Window* window, Swapchain* swapchain);
	void DrawFrame(Win32Window* window, Camera* camera, Swapchain* swapchain, Surface surface, VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void RecreateImage(Swapchain* swapchain) { CreateImage(swapchain->extent.width, swapchain->extent.height); }

	VkImage RTImage = VK_NULL_HANDLE;

private:
	void UpdateMaterialDescriptorSets();
	void CreateMaterialBuffers();
	void CreateShaderBindingTable();
	void CreateImage(uint32_t width, uint32_t height);
	void CreateBLAS(BottomLevelAccelerationStructure& BLAS, VulkanBuffer vertexBuffer, IndexBuffer indexBuffer, uint32_t vertexSize, uint32_t faceCount);
	void CreateTLAS(TopLevelAccelerationStructure& TLAS);

	BottomLevelAccelerationStructure BLAS;
	TopLevelAccelerationStructure TLAS;

	Win32Window* window;
	Swapchain* swapchain;

	VkDevice logicalDevice;

	VkDeviceMemory RTImageMemory = VK_NULL_HANDLE;
	VkImageView RTImageView = VK_NULL_HANDLE;

	VkCommandPool commandPool;
	PhysicalDevice physicalDevice;

	VkBuffer materialBuffer;
	VkDeviceMemory materialBufferMemory;

	VkBuffer materialIndexBuffer;
	VkDeviceMemory materialIndexBufferMemory;
};