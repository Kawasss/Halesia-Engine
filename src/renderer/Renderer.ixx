module;

#include "PhysicalDevice.h"
#include "Buffer.h"
#include "FramesInFlight.h"

#include "../system/CriticalSection.h"

export module Renderer;

import "../glm.h";

import std;

import Core.CameraObject;
import Core.LightObject;
import Core.Object;

import System.Window;

import IO.CreationData;

import Renderer.AnimationManager;
import Renderer.GraphicsPipeline;
import Renderer.StorageBuffer;
import Renderer.Framebuffer;
import Renderer.RenderPipeline;
import Renderer.QueryPool;
import Renderer.Swapchain;
import Renderer.Surface;
import Renderer.CommandBuffer;
import Renderer.Vertex;
import Renderer.RenderableMesh;

using HANDLE = void*;
using Handle = unsigned long long;

export using RendererFlags = std::uint32_t;
export using MeshHandle    = std::uintptr_t;

template<typename T>
concept InheritsRenderPipeline = std::is_base_of_v<RenderPipeline, T>;

class Renderer;

export class Renderer
{
public:
	enum Flags : RendererFlags
	{
		None = 0 << 0,
		NoRayTracing = 1 << 0,
		NoShaderRecompilation = 1 << 1,
		NoValidation = 1 << 2,
		NoFilteringOnResult = 1 << 3,
	};

	static constexpr std::uint32_t MAX_MESHES            = 1000U; //should be more than enough
	static constexpr std::uint32_t MAX_BINDLESS_TEXTURES = MAX_MESHES * 5; //amount of pbr textures per mesh
	static constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT  = FIF::FRAME_COUNT;
	static constexpr std::uint32_t MAX_TLAS_INSTANCES    = MAX_MESHES;

	static constexpr std::uint32_t MATERIAL_BUFFER_BINDING   = 0;
	static constexpr std::uint32_t LIGHT_BUFFER_BINDING      = 0;
	static constexpr std::uint32_t SCENE_DATA_BUFFER_BINDING = 1;

	static StorageBuffer<Vertex>        g_vertexBuffer;
	static StorageBuffer<std::uint32_t> g_indexBuffer;
	static StorageBuffer<Vertex>        g_defaultVertexBuffer;

	static VkSampler defaultSampler;
	static VkSampler noFilterSampler;

	Renderer(Window* window, RendererFlags flags);
	~Renderer();

	void RenderObjects(const std::vector<Object*>& objects, CameraObject* camera);

	void StartRecording(float delta);
	void SubmitRecording();

	void StartRenderPass(VkRenderPass renderPass, glm::vec3 clearColor = glm::vec3(0), VkFramebuffer framebuffer = VK_NULL_HANDLE);
	void StartRenderPass(const Framebuffer& framebuffer, glm::vec3 clearColor = glm::vec3(0));

	void SetViewportOffsets(glm::vec2 offsets);
	void SetViewportModifiers(glm::vec2 modifiers);

	std::map<std::string, std::uint64_t> GetTimestamps() const;

	VkRenderPass GetDefault3DRenderPass()   const;
	VkRenderPass GetNonClearingRenderPass() const;

	Framebuffer& GetFramebuffer();

	std::uint32_t GetInternalWidth()  const;
	std::uint32_t GetInternalHeight() const;

	glm::vec2 GetViewportOffset() const;
	glm::vec2 GetViewportModifier() const;

	CommandBuffer GetActiveCommandBuffer() const;

	const FIF::Buffer& GetLightBuffer() const;

	const std::vector<RenderPipeline*>& GetAllRenderPipelines() const;
	std::string_view GetRenderPipelineName(RenderPipeline* renderPipeline) const;

	int GetLightCount() const;

	void SetRenderMode(RenderMode mode);
	RenderMode GetRenderMode() const;

	MeshHandle LoadMesh(const MeshCreationData& data);
	MeshHandle CopyMeshHandle(const MeshHandle& handle); // returns the same value as 'handle'

	void SetInternalResolutionScale(float scale);
	static float GetInternalResolutionScale();

	static void BindBuffersForRendering(CommandBuffer commandBuffer);
	static void RenderMesh(CommandBuffer commandBuffer, const RenderableMesh& mesh, std::uint32_t instanceCount = 1);

	static void SetViewport(CommandBuffer commandBuffer, VkExtent2D extent);
	static void SetScissors(CommandBuffer commandBuffer, VkExtent2D extent);

	static bool CompletedFIFCyle();

	RenderPipeline::Payload GetPipelinePayload(CommandBuffer commandBuffer, CameraObject* camera);

	template<InheritsRenderPipeline Type>
	Type* AddRenderPipeline(const char* name = "unnamed pipeline"); // returns the created pipeline

	RenderPipeline* GetRenderPipeline(const std::string_view& name);

	Swapchain* swapchain; // better to keep it private
	AnimationManager* animationManager;

	static float internalScale;
	bool shouldRasterize = false;
	static bool canRayTrace;
	static Handle selectedHandle;

	std::uint32_t receivedObjects = 0;
	std::uint32_t renderedObjects = 0;
	std::uint32_t submittedCount = 0;
	std::uint32_t frameCount = 0;

	float animationTime = 0;
	float rebuildingTime = 0;
	float rayTracingTime = 0;
	float denoisingPrepTime = 0;
	float denoisingTime = 0;
	float finalRenderPassTime = 0;
	float idleTime = 0;

private:
	struct LightBuffer;
	struct SceneData;

	struct ManagedSet
	{
		void Create();
		void Destroy();

		VkDescriptorPool pool;

		VkDescriptorSetLayout singleLayout;
		VkDescriptorSetLayout fifLayout;
		VkDescriptorSet singleSet;
		std::array<VkDescriptorSet, FIF::FRAME_COUNT> fifSets;

	private:
		void CreateSingleLayout();
		void CreateFIFLayout();

		void AllocateSingleSets();
		void AllocateFIFSets();
	};

	VkInstance instance = VK_NULL_HANDLE;
	VkDevice logicalDevice = VK_NULL_HANDLE;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkRenderPass GUIRenderPass = VK_NULL_HANDLE;
	GraphicsPipeline* screenPipeline = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkDescriptorPool imGUIDescriptorPool = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;
	VkQueue computeQueue = VK_NULL_HANDLE;
	VkSampler resultSampler = VK_NULL_HANDLE; // does not need to be destroyed
	QueryPool queryPool;

	ManagedSet managedSet;

	Framebuffer framebuffer;

	FIF::Buffer lightBuffer;
	FIF::Buffer sceneBuffer;

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

	std::uint32_t viewportWidth, viewportHeight;
	glm::vec2 viewportOffsets = glm::vec2(0);
	glm::vec2 viewportTransModifiers = glm::vec2(1);
	std::uint32_t currentFrame = 0;
	std::uint32_t imageIndex = 0;
	std::uint32_t queueIndex = 0;
	RendererFlags flags = Flags::None;

	RenderMode renderMode = RenderMode::DontCare;

	bool shouldResize = false; // this uses a bool so that other threads can request the renderer to resize
	bool shouldResizePipelines = false;

	float time = 0.0f;

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

	void PermanentlyBindGlobalBuffers();

	void InitializeViewport();

	static void CreateGlobalBuffers();

	void UpdateMaterialBuffer();

	void GetQueryResults();

	void ResizeRenderPipelines();
	void OnResize();
	void AddExtensions();

	void CheckForVRAMOverflow();
	void CheckForInterference();

	void ProcessRenderPipeline(RenderPipeline* pipeline);
	void RunRenderPipelines(CommandBuffer commandBuffer, CameraObject* camera, const std::vector<RenderableMesh>& meshes);

	std::uint32_t DetectExternalTools();

	static void RenderImGUI(CommandBuffer commandBuffer);
	static void ResetImGUI();

	void ResetLightBuffer();
	void UpdateLightBuffer(const std::vector<LightObject*>& lights);

	void UpdateSceneData(CameraObject* camera);

	void UpdateScreenShaderTexture(std::uint32_t currentFrame, VkImageView imageView = VK_NULL_HANDLE);
	void RecordCommandBuffer(CommandBuffer commandBuffer, std::uint32_t imageIndex, const std::vector<RenderableMesh>& meshes, CameraObject* camera);

	void CheckForBufferResizes();

	std::uint32_t GetNextSwapchainImage(std::uint32_t frameIndex);
	void PresentSwapchainImage(std::uint32_t frameIndex, std::uint32_t imageIndex);
	void SubmitRenderingCommandBuffer(std::uint32_t frameIndex, std::uint32_t imageIndex);
};

template<InheritsRenderPipeline Type>
Type* Renderer::AddRenderPipeline(const char* name)
{
	Type* actualPtr = new Type();
	RenderPipeline* ptr = dynamic_cast<RenderPipeline*>(actualPtr);
	dbgPipelineNames[ptr] = name;
	ProcessRenderPipeline(ptr);  // should check if it derives from RenderPipeline

	return actualPtr;
}