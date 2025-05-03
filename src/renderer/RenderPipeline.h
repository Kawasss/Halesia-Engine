#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string_view>

#include "CommandBuffer.h"

class MeshObject;
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

	struct IntVariable
	{
		IntVariable() = default;
		IntVariable(const std::string_view& name, int* pValue) : name(name), pValue(pValue) {}

		std::string_view name;
		int* pValue = nullptr;

		bool IsValid() const { return pValue != nullptr; }
	};

	virtual void Start(const Payload& payload) = 0;
	virtual void Execute(const Payload& payload, const std::vector<MeshObject*>& objects) = 0;

	virtual ~RenderPipeline() {}

	virtual void Resize(const Payload& payload) {}

	virtual void SetRenderMode(RenderMode mode) { renderMode = mode; }

	virtual void OnRenderingBufferResize(const Payload& payload) {}

	virtual void ReloadShaders(const Payload& payload) {}

	virtual std::vector<IntVariable> GetIntVariables() { return {}; }

	template<typename T> T* GetChild() { return reinterpret_cast<T*>(this); }

	VkRenderPass renderPass = VK_NULL_HANDLE;

private:

protected:
	RenderMode renderMode = RenderMode::DontCare;
};