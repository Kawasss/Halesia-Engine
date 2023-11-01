#pragma once
#include <vector>
#include <string>
#include <vulkan/vulkan.h>

#include "PhysicalDevice.h"
#include "Surface.h"
#include "Swapchain.h"
#include "Texture.h"
#include "Vertex.h"
#include "Object.h"
#include "Camera.h"
#include "CreationObjects.h"
#include "RayTracing.h"

class Renderer
{
public:
	static constexpr uint32_t MAX_MESHES = 1000U; //mooore than enough
	static constexpr uint32_t MAX_BINDLESS_TEXTURES = MAX_MESHES * 5; //amount of pbr textures per mesh
	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 1;
	static constexpr uint32_t MAX_TLAS_INSTANCES = MAX_MESHES;

	static StorageBuffer<Vertex> globalVertexBuffer;
	static StorageBuffer<uint16_t> globalIndicesBuffer;
	static VkSampler defaultSampler;

	Renderer(Win32Window* window);
	void Destroy();
	void DrawFrame(const std::vector<Object*>& objects, Camera* camera, float delta);
	void RenderFPS(int FPS);
    void RenderGraph(const std::vector<uint64_t>& buffer, const char* label);
	void RenderGraph(const std::vector<float>& buffer, const char* label);
	void RenderPieGraph(std::vector<float>& data, const char* label = nullptr);
	VulkanCreationObject GetVulkanCreationObject();
	std::optional<std::string> RenderDevConsole();

	bool shouldRasterize = false;

private:
	VulkanCreationObject creationObject;

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

	PhysicalDevice physicalDevice;
	Surface surface;
	Win32Window* testWindow;
	Swapchain* swapchain;

	uint32_t currentFrame = 0;
	uint32_t queueIndex = 0;

	RayTracing* rayTracer;

	static bool initGlobalBuffers;

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
	void UpdateBindlessTextures(uint32_t currentFrame, const std::vector<Object*>& objects);
	void SetModelMatrices(uint32_t currentImage, std::vector<Object*> models); //parameter is used for potential culling, this allows for 500 meshes in view rather than in scene
	void SetViewport(VkCommandBuffer commandBuffer);
	void SetScissors(VkCommandBuffer commandBuffer);

	void UpdateUniformBuffers(uint32_t currentImage, Camera* camera);
	void RecordCommandBuffer(VkCommandBuffer lCommandBuffer, uint32_t imageIndex, std::vector<Object*> object, Camera* camera);
};