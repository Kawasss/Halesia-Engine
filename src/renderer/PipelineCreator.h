#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <array>

class PipelineBuilder
{
public:
	PipelineBuilder(const std::vector<VkPipelineShaderStageCreateInfo>& shaderStages) : stages(shaderStages) {}

	void DisableVertices(bool val) { options.noVertex = val;    } // vertices are on by default
	void DisableDepth(bool val)    { options.noDepth = val;     } // depth is on by default
	void DisableCulling(bool val)  { options.noCulling = val;   } // culling is on by default
	void DisableBlending(bool val) { options.noBlend = val;     } // blend is on by default
	void ShouldCullFront(bool val) { options.cullFront = val;   } // the back is culled by default
	void FrontIsCW(bool val)       { options.frontCW = val;     } // the front is counterclock-wise by default
	void PolygonAsLine(bool val)   { options.polygonLine = val; } // polygons are filled by default
	void WriteToDepth(bool val)    { options.writeDepth = val;  } // writes to the depth buffer by default

	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkRenderPass renderPass = VK_NULL_HANDLE;

	uint32_t attachmentCount = 0;

	VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;

	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	void* pNext = nullptr;

	VkPipeline Build();

private:
	struct Options
	{
		bool noVertex    : 1;
		bool noDepth     : 1;
		bool noCulling   : 1;
		bool noBlend     : 1;
		bool cullFront   : 1;
		bool frontCW     : 1;
		bool polygonLine : 1;
		bool writeDepth  : 1;
	};
	Options options{};

	std::vector<VkPipelineShaderStageCreateInfo> stages;
};

class RenderPassBuilder
{
public:
	template<size_t count>
	RenderPassBuilder(const std::array<VkFormat, count>& formats) : formats(formats.data(), formats.data() + count) {}

	RenderPassBuilder(const std::vector<VkFormat>& formats) : formats(formats) {}

	RenderPassBuilder(VkFormat format) 
	{ 
		formats.push_back(format);
	}

	void DisableDepth(bool val)   { options.noDepth = val;        } // depth is on by default
	void DontClearDepth(bool val) { options.dontClearDepth = val; } // depth is cleared by default
	void ClearOnLoad(bool val)    { options.clearOnLoad = val;    } // clear is not on by default

	void SetInitialLayout(VkImageLayout layout) { initialLayout = layout; }
	void SetFinalLayout(VkImageLayout layout)   { finalLayout   = layout; }

	VkRenderPass Build();

private:
	struct Options // optimal for space
	{
		bool noDepth        : 1;
		bool dontClearDepth : 1;
		bool clearOnLoad    : 1;
	};
	Options options{};
	
	static constexpr VkImageLayout INVALID_LAYOUT = static_cast<VkImageLayout>(-1);

	VkImageLayout initialLayout = INVALID_LAYOUT;
	VkImageLayout finalLayout   = INVALID_LAYOUT;

	std::vector<VkFormat> formats;
};