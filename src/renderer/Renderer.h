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
#include "Framebuffer.h"
#include "FramesInFlight.h"
#include "cuda_runtime_api.h"

class Intro;
class Camera;
class Object;
class Swapchain;
class Image;
class AnimationManager;
class DescriptorWriter;
class Window;
class RayTracingPipeline;
class ForwardPlusPipeline;
class GraphicsPipeline;
class RenderPipeline;
struct Mesh;
struct Light;

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
		NO_VALIDATION = 1 << 2,
		NO_FILTERING_ON_RESULT = 1 << 3,
	};

	static constexpr uint32_t MAX_MESHES			= 1000U; //should be more than enough
	static constexpr uint32_t MAX_BINDLESS_TEXTURES = MAX_MESHES * 5; //amount of pbr textures per mesh
	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT	= FIF::FRAME_COUNT;
	static constexpr uint32_t MAX_TLAS_INSTANCES	= MAX_MESHES;

	static StorageBuffer<Vertex>	g_vertexBuffer;
	static StorageBuffer<uint16_t>	g_indexBuffer;
	static StorageBuffer<Vertex>    g_defaultVertexBuffer;

	static VkSampler defaultSampler;
	static VkSampler noFilterSampler;

	static std::vector<VkDynamicState> dynamicStates;

	Renderer(Window* window, RendererFlags flags);
	~Renderer() { Destroy(); }

	void Destroy();
	void RecompileShaders();
	void RenderIntro(Intro* intro);
	void SetViewportOffsets(glm::vec2 offsets);
	void SetViewportModifiers(glm::vec2 modifiers);
	void AddLight(const Light& light);

	void StartRecording();
	void SubmitRecording();
	void RenderObjects(const std::vector<Object*>& objects, Camera* camera);
	void StartRenderPass(VkCommandBuffer commandBuffer, VkRenderPass renderPass, glm::vec3 clearColor = glm::vec3(0), VkFramebuffer framebuffer = VK_NULL_HANDLE);
	void EndRenderPass(VkCommandBuffer commandBuffer);
	void RenderImGUI(VkCommandBuffer commandBuffer);

	std::map<std::string, uint64_t> GetTimestamps() { return queryPool.GetTimestamps(); }

	VkRenderPass GetDefault3DRenderPass()   { return renderPass;    }
	VkRenderPass GetNonClearingRenderPass() { return GUIRenderPass; }

	Framebuffer& GetFramebuffer() { return framebuffer; }

	uint32_t GetInternalWidth()  { return viewportWidth;  }
	uint32_t GetInternalHeight() { return viewportHeight; }

	void SetInternalResolutionScale(float scale);
	static float GetInternalResolutionScale();

	static void BindBuffersForRendering(VkCommandBuffer commandBuffer);
	static void RenderMesh(VkCommandBuffer commandBuffer, const Mesh& mesh, uint32_t instanceCount = 1);

	static bool CompletedFIFCyle() { return FIF::frameIndex == 0; }

	template<typename Type> void AddRenderPipeline()
	{
		Type* actualPtr = new Type();
		RenderPipeline* ptr = dynamic_cast<RenderPipeline*>(actualPtr);
		dbgPipelineNames[ptr] = typeid(Type).name();
		ProcessRenderPipeline(ptr);  // should check if it derives from RenderPipeline
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
	VkInstance instance					 = VK_NULL_HANDLE;
	VkDevice logicalDevice				 = VK_NULL_HANDLE;
	VkRenderPass renderPass				 = VK_NULL_HANDLE;
	VkRenderPass GUIRenderPass           = VK_NULL_HANDLE;
	GraphicsPipeline* screenPipeline     = VK_NULL_HANDLE;
	VkCommandPool commandPool			 = VK_NULL_HANDLE;
	VkDescriptorPool imGUIDescriptorPool = VK_NULL_HANDLE;
	VkQueue graphicsQueue				 = VK_NULL_HANDLE;
	VkQueue presentQueue				 = VK_NULL_HANDLE;
	VkQueue computeQueue                 = VK_NULL_HANDLE;
	VkSampler resultSampler              = VK_NULL_HANDLE; // does not need to be destroyed
	QueryPool queryPool;

	Framebuffer framebuffer;

	std::unordered_map<RenderPipeline*, std::string> dbgPipelineNames;

	std::vector<VkCommandBuffer>	commandBuffers;
	std::vector<VkSemaphore>		imageAvaibleSemaphores;
	std::vector<VkSemaphore>		renderFinishedSemaphores;
	std::vector<cudaExternalSemaphore_t> externalRenderSemaphores;
	std::vector<HANDLE>             externalRenderSemaphoreHandles;
	std::vector<VkFence>			inFlightFences;

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
	
	RayTracingPipeline* rayTracer;
	ForwardPlusPipeline* fwdPlus;
	DescriptorWriter* writer;

	static bool initGlobalBuffers;

	bool shouldResize = false; // this uses a bool so that other threads can request the renderer to resize

	void InitVulkan();
	void SetLogicalDevice();
	void CreateGraphicsPipeline();
	void CreateCommandPool();
	void CreateTextureSampler();
	void CreateCommandBuffer();
	void CreateSyncObjects();
	void CreateRenderPass();
	void CreateImGUI();
	void GetQueryResults();
	void WriteTimestamp(VkCommandBuffer commandBuffer, bool reset = false);
	void UpdateBindlessTextures(uint32_t currentFrame, const std::vector<Object*>& objects);
	void SetViewport(VkCommandBuffer commandBuffer, VkExtent2D extent);
	void SetScissors(VkCommandBuffer commandBuffer, VkExtent2D extent);
	void DenoiseSynchronized(VkCommandBuffer commandBuffer);
	void ProcessRenderPipeline(RenderPipeline* pipeline);
	void ExportSemaphores();
	void OnResize();
	void AddExtensions();
	void CreateContext();
	void CreatePhysicalDevice();
	uint32_t DetectExternalTools();

	void UpdateScreenShaderTexture(uint32_t currentFrame, VkImageView imageView = VK_NULL_HANDLE);
	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, std::vector<Object*> object, Camera* camera);
	void RenderCollisionBoxes(const std::vector<Object*>& objects, VkCommandBuffer commandBuffer, uint32_t currentImage);

	void CheckForBufferResizes();

	uint32_t GetNextSwapchainImage(uint32_t frameIndex);
	void PresentSwapchainImage(uint32_t frameIndex, uint32_t imageIndex);
	void SubmitRenderingCommandBuffer(uint32_t frameIndex, uint32_t imageIndex);

	RenderPipeline::Payload GetPipelinePayload(VkCommandBuffer commandBuffer, Camera* camera);
};