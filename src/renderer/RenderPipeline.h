#pragma once
#include <vulkan/vulkan.h>
#include <vector>

#include "../glm.h"

class Object;
class Window;
class Camera;
class Renderer;

class RenderPipeline
{
public:
	struct Payload
	{
		VkCommandBuffer commandBuffer;
		Renderer* renderer;
		Window* window;
		Camera* camera;
		uint32_t width;
		uint32_t height;
	};

	virtual void Start(const Payload& payload) = 0;
	virtual void Execute(const Payload& payload, const std::vector<Object*>& objects) = 0;
	virtual void Destroy() = 0;

	virtual void AddLight(glm::vec3 pos) {}

	template<typename T> T* GetChild() { return reinterpret_cast<T*>(this); }

	VkRenderPass renderPass = VK_NULL_HANDLE;

private:
};