#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <span>

#include "Pipeline.h"

class ShaderGroupReflector;
class Swapchain;

class GraphicsPipeline : public Pipeline
{
public:
	struct CreateInfo
	{
		std::string vertexShader;
		std::string fragmentShader;
		VkRenderPass renderPass = VK_NULL_HANDLE;
		VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;

		//maybe just flags ??
		bool noVertices  = false;
		bool noDepth     = false;
		bool noCulling   = false;
		bool noBlending  = false;
		bool cullFront   = false;
		bool frontCW     = false;
		bool polygonLine = false;
		bool writeDepth  = true;
	};

	GraphicsPipeline(const CreateInfo& createInfo);
	GraphicsPipeline(const GraphicsPipeline&) = delete;
	GraphicsPipeline& operator=(GraphicsPipeline&&) = delete;

private:
	void CreatePipelineLayout(const ShaderGroupReflector& reflector);
	void CreateGraphicsPipeline(const std::span<std::span<char>>& shaders, const CreateInfo& createInfo, uint32_t attachmentCount);
};