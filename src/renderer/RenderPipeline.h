#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class Object;

class RenderPipeline
{
public:

	virtual void Execute(VkCommandBuffer commandBuffer, const std::vector<Object*>& objects) {}

	virtual void Destroy() {}

	~RenderPipeline() { Destroy(); }

private:
};