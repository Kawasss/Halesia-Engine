#pragma once
#define USE_CUDA
#ifdef USE_CUDA
#pragma comment(lib, "cudart_static.lib")
#endif
#include <vector>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "PhysicalDevice.h"
#include "Surface.h"
#include "Vertex.h"
#include "StorageBuffer.h"
#include "QueryPool.h"
#include "Buffer.h"
#include "RenderPipeline.h"
#include "cuda_runtime_api.h"

class Intro;
class Camera;
class Object;
class Swapchain;
class RayTracing;
class Image;
class AnimationManager;
class ForwardPlusRenderer;
class ForwardPlusPipeline;
class DescriptorWriter;
class Window;
class RenderPipeline;
struct Mesh;

typedef void* HANDLE;
typedef uint32_t RendererFlags;

class Renderer
{
public:
	enum Flags : RendererFlags
	{
		NONE = 0,
		NO_RAY_TRACING = 1 << 0,
		NO_SHADER_RECOMPILATION = 1 << 1,
	};

	static constexpr uint32_t MAX_MESHES			= 1000U; //should be more than enough
	static constexpr uint32_t MAX_BINDLESS_TEXTURES = MAX_MESHES * 5; //amount of pbr textures per mesh
	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT	= 1;
	static constexpr uint32_t MAX_TLAS_INSTANCES	= MAX_MESHES;

	static StorageBuffer<Vertex>	g_vertexBuffer;
	static StorageBuffer<uint16_t>	g_indexBuffer;
	static StorageBuffer<Vertex>    g_defaultVertexBuffer;

	static VkSampler defaultSampler;

	static std::vector<VkDynamicState> dynamicStates;

	Renderer(Window* window, RendererFlags flags);
	void Destroy();
	void RecompileShaders();
	void DrawFrame(const std::vector<Object*>& objects, Camera* camera, float delta);
	void RenderIntro(Intro* intro);
	void SetViewportOffsets(glm::vec2 offsets);
	void SetViewportModifiers(glm::vec2 modifiers);

	void StartRecording();
	void SubmitRecording();
	void RenderObjects(const std::vector<Object*>& objects, Camera* camera);

	void SetInternalResolutionScale(float scale);
	static float GetInternalResolutionScale();

	template<typename Type> void AddRenderPipeline()
	{
		RenderPipeline* ptr = dynamic_cast<RenderPipeline*>(new Type());
		renderPipelines.push_back(ptr); // should check if it derives from RenderPipeline
	}

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
	struct UniformBufferObject
	{
		glm::vec3 cameraPos;

		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 projection;
		uint32_t width;
		uint32_t height;
	};

	struct ModelData
	{
		glm::mat4 transformation;
		glm::vec4 IDColor;
	};

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
	QueryPool queryPool;

	std::vector<VkCommandBuffer>	commandBuffers;
	std::vector<VkSemaphore>		imageAvaibleSemaphores;
	std::vector<VkSemaphore>		renderFinishedSemaphores;
	std::vector<cudaExternalSemaphore_t> externalRenderSemaphores;
	std::vector<HANDLE>             externalRenderSemaphoreHandles;
	std::vector<VkFence>			inFlightFences;
	std::vector<VkDescriptorSet>	descriptorSets;

	std::array<Buffer, MAX_FRAMES_IN_FLIGHT>               uniformBuffers;
	std::array<UniformBufferObject*, MAX_FRAMES_IN_FLIGHT> uniformBuffersMapped;

	std::array<Buffer, MAX_FRAMES_IN_FLIGHT>     modelBuffers;
	std::array<ModelData*, MAX_FRAMES_IN_FLIGHT> modelBuffersMapped;

	StorageBuffer<VkDrawIndexedIndirectCommand> indirectDrawParameters;
	std::unordered_map<int, Handle> processedMaterials;

	std::mutex drawingMutex;

	std::vector<RenderPipeline*> renderPipelines; // owns the pointers !!

	PhysicalDevice physicalDevice;
	Surface surface;
	Window* testWindow;

	uint32_t viewportWidth, viewportHeight;
	glm::vec2 viewportOffsets = glm::vec2(0);
	glm::vec2 viewportTransModifiers = glm::vec2(1);
	uint32_t currentFrame = 0;
	uint32_t imageIndex = 0;
	uint32_t queueIndex = 0;
	RendererFlags flags = NONE; 
	
	RayTracing* rayTracer;
	ForwardPlusPipeline* fwdPlus;
	DescriptorWriter* writer;

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
	void AddExtensions();
	void CreateContext();

	void UpdateScreenShaderTexture(uint32_t currentFrame, VkImageView imageView = VK_NULL_HANDLE);
	void UpdateUniformBuffers(uint32_t currentImage, Camera* camera);
	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, std::vector<Object*> object, Camera* camera);
	void RenderCollisionBoxes(const std::vector<Object*>& objects, VkCommandBuffer commandBuffer, uint32_t currentImage);
	void RasterizeObjects(VkCommandBuffer commandBuffer, const std::vector<Object*>& objects);

	uint32_t GetNextSwapchainImage(uint32_t frameIndex);
	void PresentSwapchainImage(uint32_t frameIndex, uint32_t imageIndex);
	void SubmitRenderingCommandBuffer(uint32_t frameIndex, uint32_t imageIndex);

	RenderPipeline::Payload GetPipelinePayload(VkCommandBuffer commandBuffer, Camera* camera);
};