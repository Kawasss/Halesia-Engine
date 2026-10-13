export module Renderer.DynamicRendering;

import <vulkan/vulkan.h>;

import Renderer.CommandBuffer;
import Renderer.Texture;

import std;

export class DynamicAttachment
{
public:
	/// <summary>
	/// If info.sType is not VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, it will initialize the info to default values
	/// </summary>
	/// <param name="info"></param>
	DynamicAttachment(VkRenderingAttachmentInfo& info);

	void SetImage(const Image& img);

	/// <summary>
	/// Must be set manually with this function or the SetImage function
	/// </summary>
	/// <returns></returns>
	VkImageView& View();

	/// <summary>
	/// Must be set manually
	/// </summary>
	/// <returns></returns>
	VkImageLayout&         Layout();

	/// <summary>
	/// Default value is VK_RESOLVE_MODE_NONE
	/// </summary>
	/// <returns></returns>
	VkResolveModeFlagBits& ResolveMode();

	/// <summary>
	/// Default value is VK_ATTACHMENT_LOAD_OP_DONT_CARE
	/// </summary>
	/// <returns></returns>
	VkAttachmentLoadOp&    LoadOp();

	/// <summary>
	/// Default value is VK_ATTACHMENT_STORE_OP_STORE
	/// </summary>
	/// <returns></returns>
	VkAttachmentStoreOp&   StoreOp();

	/// <summary>
	/// Default value is all 0
	/// </summary>
	/// <returns></returns>
	VkClearColorValue& ColorClear();

	/// <summary>
	/// Default value is all 0
	/// </summary>
	/// <returns></returns>
	VkClearDepthStencilValue& DepthStencilClear();

private:
	void SetDefaultValues();

	VkRenderingAttachmentInfo& info;
};

export class DynamicRender
{
public:
	DynamicRender() = default;
	DynamicRender(const std::span<const Image>& images);

	DynamicAttachment Color(std::size_t index);
	DynamicAttachment Depth();
	DynamicAttachment Stencil();

	VkOffset2D& Offset();

	/// <summary>
	/// When the images constructor is used, this is set to the dimensions of the given images.
	/// </summary>
	/// <returns></returns>
	VkExtent2D& Extent();

	bool Begin(const CommandBuffer& cmdBuffer) const;
	void End(const CommandBuffer& cmdBuffer) const;

private:
	static const VkRenderingAttachmentInfo* GetPointer(const std::optional<VkRenderingAttachmentInfo>& optImage);

	static bool IsValidAttachment(const std::optional<VkRenderingAttachmentInfo>& info);
	static bool IsValidAttachment(const VkRenderingAttachmentInfo& info);

	std::optional<VkRenderingInfo> GetRenderingInfo() const;

    std::vector<VkRenderingAttachmentInfo>   color; // not happy with this vector, would like if it was an array
	std::optional<VkRenderingAttachmentInfo> depth;
	std::optional<VkRenderingAttachmentInfo> stencil;

	VkRect2D area{};
};