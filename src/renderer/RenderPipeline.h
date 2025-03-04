#pragma once
#include <vulkan/vulkan.h>
#include <vector>

#include "CommandBuffer.h"

class Object;
class Window;
class Camera;
class Renderer;
struct Light;

enum class RenderMode : uint8_t // this enum is used as a suggestion
{
	DontCare,
	Albedo,
	Normal,
	Metallic,
	Roughness,
	AmbientOcclusion,
	Polygon,
	UV,
};

class RenderPipeline
{
public:
	struct Payload
	{
		CommandBuffer commandBuffer;
		Renderer* renderer;
		Window* window;
		Camera* camera;
		uint32_t width;
		uint32_t height;
	};

	virtual void Start(const Payload& payload) = 0;
	virtual void Execute(const Payload& payload, const std::vector<Object*>& objects) = 0;

	virtual ~RenderPipeline() {}

	virtual void Resize(const Payload& payload) {}

	virtual void SetRenderMode(RenderMode mode) { renderMode = mode; }

	virtual void OnRenderingBufferResize(const Payload& payload) {}

	virtual void ReloadShaders(const Payload& payload) {}

	template<typename T> T* GetChild() { return reinterpret_cast<T*>(this); }

	VkRenderPass renderPass = VK_NULL_HANDLE;

private:

protected:
	RenderMode renderMode = RenderMode::DontCare;
};