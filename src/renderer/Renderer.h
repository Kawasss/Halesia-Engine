#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <map>
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
#include "CommandBuffer.h"

class Intro;
class Camera;
class Object;
class Swapchain;
class Image;
class AnimationManager;
class Window;
class LightObject;
class RayTracingRenderPipeline;
class ForwardPlusPipeline;
class GraphicsPipeline;
class RenderPipeline;
struct Mesh;
struct Light;
struct Material;

using HANDLE = void*;
using Handle = unsigned long long;
using RendererFlags = uint32_t;

class Renderer
{
public:
	enum Flags : RendererFlags
	{
		None                  = 0 << 0,
		NoRayTracing          = 1 << 0,
		NoShaderRecompilation = 1 << 1,
		NoValidation          = 1 << 2,
		NoFilteringOnResult   = 1 << 3,
	};

	static constexpr uint32_t MAX_MESHES			= 1000U; //should be more than enough
	static constexpr uint32_t MAX_BINDLESS_TEXTURES = MAX_MESHES * 5; //amount of pbr textures per mesh
	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT	= FIF::FRAME_COUNT;
	static constexpr uint32_t MAX_TLAS_INSTANCES	= MAX_MESHES;

	static constexpr uint32_t RESERVED_DESCRIPTOR_SET = 3;
	static constexpr uint32_t MATERIAL_BUFFER_BINDING = 0;

	static StorageBuffer<Vertex>   g_vertexBuffer;
	static StorageBuffer<uint32_t> g_indexBuffer;
	static StorageBuffer<Vertex>   g_defaultVertexBuffer;

	static VkSampler defaultSampler;
	static VkSampler noFilterSampler;

	Renderer(Window* window, RendererFlags flags);
	~Renderer();

	void RenderIntro(Intro* intro);
	void RenderObjects(const std::vector<Object*>& objects, Camera* camera);

	void StartRecording();
	void SubmitRecording();

	void StartRenderPass(VkRenderPass renderPass, glm::vec3 clearColor = glm::vec3(0), VkFramebuffer framebuffer = VK_NULL_HANDLE);
	void StartRenderPass(const Framebuffer& framebuffer, glm::vec3 clearColor = glm::vec3(0));

	void SetViewportOffsets(glm::vec2 offsets);
	void SetViewportModifiers(glm::vec2 modifiers);

	std::map<std::string, uint64_t> GetTimestamps() const;

	VkRenderPass GetDefault3DRenderPass()   const;
	VkRenderPass GetNonClearingRenderPass() const;

	Framebuffer& GetFramebuffer();

	uint32_t GetInternalWidth()  const;
	uint32_t GetInternalHeight() const;

	glm::vec2 GetViewportOffset() const;
	glm::vec2 GetViewportModifier() const;

	CommandBuffer GetActiveCommandBuffer() const;
	
	const FIF::Buffer& GetLightBuffer() const;

	const std::vector<RenderPipeline*>& GetAllRenderPipelines() const;
	std::string_view GetRenderPipelineName(RenderPipeline* renderPipeline) const;

	int GetLightCount() const;

	void SetRenderMode(RenderMode mode);
	RenderMode GetRenderMode() const;

	void SetInternalResolutionScale(float scale);
	static float GetInternalResolutionScale();

	static void BindBuffersForRendering(CommandBuffer commandBuffer);
	static void RenderMesh(CommandBuffer commandBuffer, const Mesh& mesh, uint32_t instanceCount = 1);

	static void SetViewport(CommandBuffer commandBuffer, VkExtent2D extent);
	static void SetScissors(CommandBuffer commandBuffer, VkExtent2D extent);

	static bool CompletedFIFCyle();

	RenderPipeline::Payload GetPipelinePayload(CommandBuffer commandBuffer, Camera* camera);

	template<typename Type> 
	Type* AddRenderPipeline(const char* name = "unnamed pipeline"); // returns the created pipeline

	RenderPipeline* GetRenderPipeline(const std::string_view& name);
	
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
	uint32_t frameCount = 0;

	float animationTime = 0;
	float rebuildingTime = 0;
	float rayTracingTime = 0;
	float denoisingPrepTime = 0;
	float denoisingTime = 0;
	float finalRenderPassTime = 0;
	float idleTime = 0;

private:
	struct LightBuffer;

	struct RendererManagedSet
	{
		void Create();
		void Destroy();

		VkDescriptorPool pool;
		VkDescriptorSet set;
		VkDescriptorSetLayout layout;
	};

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

	RendererManagedSet managedSet;

	Framebuffer framebuffer;

	FIF::Buffer lightBuffer;

	std::map<RenderPipeline*, std::string> dbgPipelineNames;

	std::vector<CommandBuffer> commandBuffers;
	std::vector<VkSemaphore>   imageAvaibleSemaphores;
	std::vector<VkSemaphore>   renderFinishedSemaphores;
	std::vector<VkFence>       inFlightFences;

	CommandBuffer activeCmdBuffer = VK_NULL_HANDLE;

	win32::CriticalSection drawingSection;

	std::vector<RenderPipeline*> renderPipelines; // owns the pointers !!

	std::vector<Handle> materials;

	PhysicalDevice physicalDevice;
	Surface surface;
	Window* testWindow;

	uint32_t viewportWidth, viewportHeight;
	glm::vec2 viewportOffsets = glm::vec2(0);
	glm::vec2 viewportTransModifiers = glm::vec2(1);
	uint32_t currentFrame = 0;
	uint32_t imageIndex = 0;
	uint32_t queueIndex = 0;
	RendererFlags flags = Flags::None; 
	
	RayTracingRenderPipeline* rayTracer;
	ForwardPlusPipeline* fwdPlus;

	RenderMode renderMode = RenderMode::DontCare;

	bool shouldResize = false; // this uses a bool so that other threads can request the renderer to resize

	void Destroy();

	void InitVulkan();
	void SetLogicalDevice();

	void CreateContext();
	void CreatePhysicalDevice();
	void CreateCommandPool();
	void CreateTextureSampler();
	void CreateCommandBuffer();
	void CreateSyncObjects();
	void Create3DRenderPass();
	void CreateGUIRenderPass();
	void CreateSwapchain();
	void CreateImGUI();
	void CreateDefaultObjects();

	void InitializeViewport();

	static void CreateGlobalBuffers();

	void UpdateMaterialBuffer();

	void GetQueryResults();
	
	void OnResize();
	void AddExtensions();
	
	void CheckForVRAMOverflow();
	void CheckForInterference();

	void ProcessRenderPipeline(RenderPipeline* pipeline);
	void RunRenderPipelines(CommandBuffer commandBuffer, Camera* camera, const std::vector<MeshObject*>& objects);

	uint32_t DetectExternalTools();

	static void RenderImGUI(CommandBuffer commandBuffer);
	static void ResetImGUI();

	void ResetLightBuffer();
	void UpdateLightBuffer(const std::vector<LightObject*>& lights);

	void UpdateScreenShaderTexture(uint32_t currentFrame, VkImageView imageView = VK_NULL_HANDLE);
	void RecordCommandBuffer(CommandBuffer commandBuffer, uint32_t imageIndex, std::vector<MeshObject*> object, Camera* camera);
	void RenderCollisionBoxes(const std::vector<Object*>& objects, VkCommandBuffer commandBuffer, uint32_t currentImage);

	void CheckForBufferResizes();

	uint32_t GetNextSwapchainImage(uint32_t frameIndex);
	void PresentSwapchainImage(uint32_t frameIndex, uint32_t imageIndex);
	void SubmitRenderingCommandBuffer(uint32_t frameIndex, uint32_t imageIndex);
};

template<typename Type>
Type* Renderer::AddRenderPipeline(const char* name)
{
	Type* actualPtr = new Type();
	RenderPipeline* ptr = dynamic_cast<RenderPipeline*>(actualPtr);
	dbgPipelineNames[ptr] = name;
	ProcessRenderPipeline(ptr);  // should check if it derives from RenderPipeline

	return actualPtr;
}