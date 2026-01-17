module;

#include "Pipeline.h"

export module Renderer.GraphicsPipeline;

import std;

import Renderer.ShaderReflector;

export class GraphicsPipeline : public Pipeline
{
public:
	struct CreateInfo
	{
		std::string vertexShader;
		std::string fragmentShader;
		VkRenderPass renderPass = VK_NULL_HANDLE;
		VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
		VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// only enter when using dynamic rendering
		std::vector<VkFormat> colorFormats;
		VkFormat depthStencilFormat = VK_FORMAT_UNDEFINED;

		//maybe just flags ??
		bool noVertices = false;
		bool noDepth = false;
		bool noCulling = false;
		bool noBlending = false;
		bool cullFront = false;
		bool frontCW = false;
		bool polygonLine = false;
		bool writeDepth = true;
	};

	GraphicsPipeline(const CreateInfo& createInfo);
	GraphicsPipeline(const GraphicsPipeline&) = delete;
	GraphicsPipeline& operator=(GraphicsPipeline&&) = delete;

private:
	void CreatePipelineLayout(const ShaderGroupReflector& reflector);
	void CreateGraphicsPipeline(const std::span<std::span<char>>& shaders, const CreateInfo& createInfo, std::uint32_t attachmentCount);
};