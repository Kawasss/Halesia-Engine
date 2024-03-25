#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "PhysicalDevice.h"
#include "Surface.h"
#include "Vertex.h"
#include "StorageBuffer.h"
#include "cuda_runtime_api.h"

class Intro;
class Camera;
class Object;
class Swapchain;
class RayTracing;
class Image;
class AnimationManager;
struct Mesh;

typedef void* HANDLE;

class Renderer
{
public:
	static constexpr uint32_t MAX_MESHES			= 1000U; //should be more than enough
	static constexpr uint32_t MAX_BINDLESS_TEXTURES = MAX_MESHES * 5; //amount of pbr textures per mesh
	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT	= 1;
	static constexpr uint32_t MAX_TLAS_INSTANCES	= MAX_MESHES;

	static StorageBuffer<Vertex>	g_vertexBuffer;
	static StorageBuffer<uint16_t>	g_indexBuffer;
	static StorageBuffer<Vertex>    g_defaultVertexBuffer;

	static VkSampler defaultSampler;

	static std::vector<VkDynamicState> dynamicStates;

	Renderer(Win32Window* window);
	void Destroy();
	void RecompileShaders();
	void DrawFrame(const std::vector<Object*>& objects, Camera* camera, float delta);
	void RenderIntro(Intro* intro);
	void SetViewportOffsets(glm::vec2 offsets);
	void SetViewportModifiers(glm::vec2 modifiers);

	void SetInternalResolutionScale(float scale);
	static float GetInternalResolutionScale();

	Swapchain* swapchain; // better to keep it private
	AnimationManager* animationManager;

	static float internalScale;
	bool shouldRasterize = false;
	static bool canRayTrace;
	static bool shouldRenderCollisionBoxes;
	static bool denoiseOutput;
	static Handle selectedHandle;

	uint32_t receivedObjects = 0;
	uint32_t renderedObjects = 0;
	uint32_t submittedCount = 0;

	float animationTime = 0;
	float rebuildingTime = 0;
	float rayTracingTime = 0;
	float denoisingPrepTime = 0;
	float denoisingTime = 0;
	float finalRenderPassTime = 0;
	float idleTime = 0;

private:
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
	VkQueue computeQueue                        = VK_NULL_HANDLE;
	VkQueryPool queryPool                       = VK_NULL_HANDLE;

	std::vector<VkCommandBuffer>	commandBuffers;
	std::vector<VkSemaphore>		imageAvaibleSemaphores;
	std::vector<VkSemaphore>		renderFinishedSemaphores;
	std::vector<cudaExternalSemaphore_t> externalRenderSemaphores;
	std::vector<HANDLE>             externalRenderSemaphoreHandles;
	std::vector<VkFence>			inFlightFences;
	std::vector<VkDescriptorSet>	descriptorSets;

	std::vector<VkBuffer>			uniformBuffers;
	std::vector<VkDeviceMemory>		uniformBuffersMemory;
	std::vector<void*>				uniformBuffersMapped;

	std::vector<VkBuffer>			modelBuffers;
	std::vector<VkDeviceMemory>		modelBuffersMemory;
	std::vector<void*>				modelBuffersMapped;

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
	void GetQueryResults();
	void WriteTimestamp(VkCommandBuffer commandBuffer, bool reset = false);
	void WriteIndirectDrawParameters(std::vector<Object*>& objects);
	void UpdateBindlessTextures(uint32_t currentFrame, const std::vector<Object*>& objects);
	void SetModelData(uint32_t currentImage, const std::vector<Object*>& objects); //parameter is used for potential culling, this allows for 500 meshes in view rather than in scene
	void SetViewport(VkCommandBuffer commandBuffer);
	void SetScissors(VkCommandBuffer commandBuffer);
	void DenoiseSynchronized(VkCommandBuffer commandBuffer);
	void ExportSemaphores();
	void DetectExternalTools();
	void OnResize();

	void UpdateScreenShaderTexture(uint32_t currentFrame, VkImageView imageView = VK_NULL_HANDLE);
	void UpdateUniformBuffers(uint32_t currentImage, Camera* camera);
	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, std::vector<Object*> object, Camera* camera);
	void RenderCollisionBoxes(const std::vector<Object*>& objects, VkCommandBuffer commandBuffer, uint32_t currentImage);
	void RasterizeObjects(VkCommandBuffer commandBuffer, const std::vector<Object*>& objects);

	uint32_t GetNextSwapchainImage(uint32_t frameIndex);
	void PresentSwapchainImage(uint32_t frameIndex, uint32_t imageIndex);
	void SubmitRenderingCommandBuffer(uint32_t frameIndex, uint32_t imageIndex);
};