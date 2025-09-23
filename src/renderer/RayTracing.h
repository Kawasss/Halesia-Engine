#pragma once
#include <unordered_map>
#include <memory>
#include <array>

#include "Buffer.h"
#include "RenderPipeline.h"
#include "glm.h"

using Handle = uint64_t;
class BottomLevelAccelerationStructure;
class TopLevelAccelerationStructure;
class Window;
class Object;
class CameraObject;
class Denoiser;
class ShaderGroupReflector;
struct InstanceMeshData;

class RayTracingRenderPipeline : public RenderPipeline
{
public:
	~RayTracingRenderPipeline();

	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<MeshObject*>& objects) override;

	void OnRenderingBufferResize(const Payload& payload) override;

	void RecreateImage(uint32_t width, uint32_t height);
	void ApplyDenoisedImage(VkCommandBuffer commandBuffer);
	void PrepareForDenoising(VkCommandBuffer commandBuffer);
	void DenoiseImage();

	std::array<vvm::Image, 3> gBuffers{};
	std::array<VkImageView, 3> gBufferViews;
	uint64_t* handleBufferMemPointer = nullptr;

	static int  raySampleCount;
	static int  rayDepth;
	static bool showNormals; // replace these 3 with an enum for render mode
	static bool showUniquePrimitives;
	static bool showAlbedo;
	static bool renderProgressive;
	static bool useWhiteAsAlbedo;
	static glm::vec3 directionalLightDir;

	VkSemaphore externSemaphore;

private:
	void UpdateInstanceDataBuffer(const std::vector<MeshObject*>& objects, CameraObject* camera);
	void UpdateTextureBuffer();
	void UpdateMeshDataDescriptorSets();
	void CreateShaderBindingTable();
	void CreateImage(uint32_t width, uint32_t height);
	void UpdateDescriptorSets();

	void Init();
	void SetUp();
	void CreateDescriptorPool(const ShaderGroupReflector& groupReflection);
	void CreateDescriptorSets(const ShaderGroupReflector& groupReflection);
	void CreateRayTracingPipeline(const std::vector<std::vector<char>>& shaderCodes);
	void CreateBuffers(const ShaderGroupReflector& groupReflection);
	void CopyPreviousResult(VkCommandBuffer commandBuffer);
	void CreateMotionBuffer();

	uint32_t amountOfActiveObjects = 0;
	uint32_t width = 0, height = 0;

	std::vector<BottomLevelAccelerationStructure*> BLASs;
	std::unique_ptr<TopLevelAccelerationStructure> TLAS;

	bool imageHasChanged = false;

	VkDevice logicalDevice = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;

	Buffer shaderBindingTableBuffer;
	Buffer handleBuffer;
	Buffer uniformBufferBuffer;
	Buffer motionBuffer;
	Buffer instanceMeshDataBuffer;
	InstanceMeshData* instanceMeshDataPointer = nullptr;

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout materialSetLayout;
	std::vector<VkDescriptorSet> descriptorSets;

	VkStridedDeviceAddressRegionKHR rchitShaderBindingTable{};
	VkStridedDeviceAddressRegionKHR rgenShaderBindingTable{};
	VkStridedDeviceAddressRegionKHR rmissShaderBindingTable{};
	VkStridedDeviceAddressRegionKHR callableShaderBindingTable{};

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };

	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

	vvm::Image prevImage;
	VkImageView prevImageView;

	std::unordered_map<int, Handle> processedMaterials;
};