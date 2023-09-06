#pragma once
#include <windows.h>
#include <vector>
#include <array>
#include <string>
#include <vulkan/vulkan.h>
#include <chrono>

#include "PhysicalDevice.h"
#include "Surface.h"
#include "Swapchain.h"
#include "Texture.h"
#include "Buffers.h"
#include "glm.h"
#include "Vertex.h"
#include "Object.h"
#include "Camera.h"
#include "CreationObjects.h"

class Renderer
{
public:
	Renderer(Win32Window* window);
	void Destroy();
	void DrawFrame(const std::vector<Object*>& objects, Camera* camera, float delta);
	void RenderFPS(int FPS);
    void RenderGraph(const std::vector<uint64_t>& buffer, const char* label);
	void RenderGraph(const std::vector<float>& buffer, const char* label);
	MeshCreationObjects GetMeshCreationObjects();
	std::optional<std::string> RenderDevConsole(bool render);

private:
	VkInstance instance{};
	VkDevice logicalDevice{};
	VkRenderPass renderPass{};
	VkDescriptorSetLayout descriptorSetLayout{};
	VkPipelineLayout pipelineLayout{};
	VkPipeline graphicsPipeline{};
	VkCommandPool commandPool{};
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkSemaphore> imageAvaibleSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	VkQueue graphicsQueue{};
	VkQueue presentQueue{};

	VkDescriptorPool descriptorPool{};
	std::vector<VkDescriptorSet> descriptorSets;
	VkDescriptorPool imGUIDescriptorPool;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;

	std::vector<VkBuffer> modelBuffers;
	std::vector<VkDeviceMemory> modelBuffersMemory;
	std::vector<void*> modelBuffersMapped;

	Texture* textureImage;
	VkSampler textureSampler;

	PhysicalDevice physicalDevice;
	Surface surface;
	Win32Window* testWindow;
	Swapchain* swapchain;

	uint32_t currentFrame = 0;

	HANDLE queueMutex;

	static std::vector<char> ReadFile(const std::string& filePath);

	void InitVulkan();
	void SetLogicalDevice();
	void CreateDescriptorSetLayout();
	void CreateGraphicsPipeline();
	void CreateCommandPool();
	void CreateTextureSampler();
	void CreateUniformBuffers();
	void CreateDescriptorPool();
	void CreateDescriptorSets();
	void CreateCommandBuffer();
	void CreateSyncObjects();
	void CreateRenderPass();
	void CreateModelBuffers();
	void CreateImGUI();
	void SetModelMatrices(uint32_t currentImage, std::vector<Object*> models); //parameter is used for potential culling, this allows for 500 meshes in view rather than in scene

	void UpdateUniformBuffers(uint32_t currentImage, Camera* camera);
	void RecordCommandBuffer(VkCommandBuffer lCommandBuffer, uint32_t imageIndex, std::vector<Object*> object);
	VkShaderModule CreateShaderModule(const std::vector<char>& code);
};