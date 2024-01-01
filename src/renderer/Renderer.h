#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "PhysicalDevice.h"
#include "Surface.h"
#include "Vertex.h"
#include "CreationObjects.h"
#include "StorageBuffer.h"

class Intro;
class Camera;
class Object;
class Swapchain;
class RayTracing;
class Image;
struct Mesh;

typedef void* HANDLE;

class Renderer
{
public:
	static constexpr uint32_t MAX_MESHES			= 1000U; //should be more than enough
	static constexpr uint32_t MAX_BINDLESS_TEXTURES = MAX_MESHES * 5; //amount of pbr textures per mesh
	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT	= 1;
	static constexpr uint32_t MAX_TLAS_INSTANCES	= MAX_MESHES;

	static StorageBuffer<Vertex>	globalVertexBuffer;
	static StorageBuffer<uint16_t>	globalIndicesBuffer;

	static VkSampler defaultSampler;

	static std::vector<VkDynamicState> dynamicStates;

	Renderer(Win32Window* window);
	void Destroy();
	void DrawFrame(const std::vector<Object*>& objects, Camera* camera, float delta);
	void RenderMeshData(const std::vector<Mesh>& meshes);
	void RenderIntro(Intro* intro);
	void SetViewportOffsets(glm::vec2 offsets);
	void SetViewportModifiers(glm::vec2 modifiers);
	VulkanCreationObject& GetVulkanCreationObject();

	Swapchain* swapchain; // better to keep it private

	bool shouldRasterize = false;
	static bool shouldRenderCollisionBoxes;
	static Handle selectedHandle;

private:
	VulkanCreationObject creationObject;

	VkInstance instance							= VK_NULL_HANDLE;
	VkDevice logicalDevice						= VK_NULL_HANDLE;
	VkRenderPass renderPass						= VK_NULL_HANDLE;
	VkRenderPass deferredRenderPass				= VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout	= VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout				= VK_NULL_HANDLE;
	VkPipeline graphicsPipeline					= VK_NULL_HANDLE;
	VkPipeline screenPipeline                   = VK_NULL_HANDLE;
	VkCommandPool commandPool					= VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool				= VK_NULL_HANDLE;
	VkDescriptorPool imGUIDescriptorPool		= VK_NULL_HANDLE;
	VkQueue graphicsQueue						= VK_NULL_HANDLE;
	VkQueue presentQueue						= VK_NULL_HANDLE;
	VkFramebuffer deferredFramebuffer			= VK_NULL_HANDLE;
	VkImageView	deferredDepthView				= VK_NULL_HANDLE;
	VkImage	deferredDepth						= VK_NULL_HANDLE;
	VkDeviceMemory deferredDepthMemory			= VK_NULL_HANDLE;

	std::vector<VkCommandBuffer>	commandBuffers;
	std::vector<VkSemaphore>		imageAvaibleSemaphores;
	std::vector<VkSemaphore>		renderFinishedSemaphores;
	//std::vector<cudaExternalSemaphore_t> externalRenderSemaphores;
	//std::vector<HANDLE>             externalRenderSemaphoreHandles;
	std::vector<VkFence>			inFlightFences;
	std::vector<VkDescriptorSet>	descriptorSets;

	std::vector<VkBuffer>			uniformBuffers;
	std::vector<VkDeviceMemory>		uniformBuffersMemory;
	std::vector<void*>				uniformBuffersMapped;

	std::vector<VkBuffer>			modelBuffers;
	std::vector<VkDeviceMemory>		modelBuffersMemory;
	std::vector<void*>				modelBuffersMapped;

	std::vector<VkImage>			gBufferImages;
	std::vector<VkImageView>		gBufferViews;
	std::vector<VkDeviceMemory>		gBufferMemories;

	StorageBuffer<VkDrawIndexedIndirectCommand> indirectDrawParameters;
	std::unordered_map<int, Handle> processedMaterials;

	std::mutex drawingMutex;

	PhysicalDevice physicalDevice;
	Surface surface;
	Win32Window* testWindow;

	uint32_t viewportWidth, viewportHeight;
	glm::vec2 viewportOffsets = glm::vec2(0);
	glm::vec2 viewportTransModifiers = glm::vec2(1);
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
	void CreateModelDataBuffers();
	void CreateImGUI();
	void CreateIndirectDrawParametersBuffer();
	void WriteIndirectDrawParameters(std::vector<Object*>& objects);
	void CreateDeferredFramebuffer(uint32_t width, uint32_t height);
	void UpdateBindlessTextures(uint32_t currentFrame, const std::vector<Object*>& objects);
	void SetModelData(uint32_t currentImage, std::vector<Object*> objects); //parameter is used for potential culling, this allows for 500 meshes in view rather than in scene
	void SetViewport(VkCommandBuffer commandBuffer);
	void SetScissors(VkCommandBuffer commandBuffer);
	void OnResize();

	void UpdateScreenShaderTexture(uint32_t currentFrame);
	void UpdateUniformBuffers(uint32_t currentImage, Camera* camera);
	void RecordCommandBuffer(VkCommandBuffer lCommandBuffer, uint32_t imageIndex, std::vector<Object*> object, Camera* camera);
	void RenderCollisionBoxes(std::vector<Object*>& objects, VkCommandBuffer commandBuffer, uint32_t currentImage);

	uint32_t GetNextSwapchainImage(uint32_t frameIndex);
	void PresentSwapchainImage(uint32_t frameIndex, uint32_t imageIndex);
	void SubmitRenderingCommandBuffer(uint32_t frameIndex, uint32_t imageIndex);
};