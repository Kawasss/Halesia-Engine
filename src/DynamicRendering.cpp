module;

#include <vulkan/vulkan.h>

module Renderer.DynamicRendering;

import std;

DynamicAttachment::DynamicAttachment(VkRenderingAttachmentInfo& info) : info(info)
{
	if (info.sType != VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO)
		SetDefaultValues();
}

void DynamicAttachment::SetDefaultValues()
{
	info.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	info.imageLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
	info.resolveMode = VK_RESOLVE_MODE_NONE;
	info.loadOp      = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	info.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
}

void DynamicAttachment::SetImage(const Image& img)
{
	info.imageView = img.view;
}

VkImageView& DynamicAttachment::View()
{
	return info.imageView;
}

VkImageLayout& DynamicAttachment::Layout()
{
	return info.imageLayout;
}

VkResolveModeFlagBits& DynamicAttachment::ResolveMode()
{
	return info.resolveMode;
}

VkAttachmentLoadOp& DynamicAttachment::LoadOp()
{
	return info.loadOp;
}

VkAttachmentStoreOp& DynamicAttachment::StoreOp()
{
	return info.storeOp;
}

VkClearColorValue& DynamicAttachment::ColorClear()
{
	return info.clearValue.color;
}

VkClearDepthStencilValue& DynamicAttachment::DepthStencilClear()
{
	return info.clearValue.depthStencil;
}

DynamicRender::DynamicRender(const std::span<const Image>& images)
{
	if (images.empty())
		return;

	color.resize(images.size());
	for (int i = 0; i < color.size(); i++)
		color[i].imageView = images[i].view;

	area.extent = { images[0].GetWidth(), images[0].GetHeight() };
}

DynamicAttachment DynamicRender::Color(std::size_t index)
{
	if (color.size() <= index)
		color.resize(index + 1);
	return color[index];
}

DynamicAttachment DynamicRender::Depth()
{
	if (!depth.has_value())
		depth = VkRenderingAttachmentInfo{};
	return *depth;
}

DynamicAttachment DynamicRender::Stencil()
{
	if (!stencil.has_value())
		stencil = VkRenderingAttachmentInfo{};
	return *stencil;
}

VkOffset2D& DynamicRender::Offset()
{
	return area.offset;
}

VkExtent2D& DynamicRender::Extent()
{
	return area.extent;
}

bool DynamicRender::Begin(const CommandBuffer& cmdBuffer) const
{
	std::optional<VkRenderingInfo> info = GetRenderingInfo();
	if (!info.has_value())
		return false;

	cmdBuffer.BeginRendering(*info);
	return true;
}

void DynamicRender::End(const CommandBuffer& cmdBuffer) const
{
	cmdBuffer.EndRendering();
}

const VkRenderingAttachmentInfo* DynamicRender::GetPointer(const std::optional<VkRenderingAttachmentInfo>& optImage)
{
	return optImage.has_value() ? &optImage.value() : nullptr;
}

bool DynamicRender::IsValidAttachment(const std::optional<VkRenderingAttachmentInfo>& info)
{
	return info.has_value() ? IsValidAttachment(*info) : true;
}

bool DynamicRender::IsValidAttachment(const VkRenderingAttachmentInfo& info)
{
	return info.imageLayout != VK_IMAGE_LAYOUT_MAX_ENUM && info.imageView != VK_NULL_HANDLE;
}

std::optional<VkRenderingInfo> DynamicRender::GetRenderingInfo() const
{
	if (!IsValidAttachment(depth) || !IsValidAttachment(stencil))
		return std::optional<VkRenderingInfo>();

	for (const VkRenderingAttachmentInfo& img : color)
	{
		if (!IsValidAttachment(img))
			return std::optional<VkRenderingInfo>();
	}

	VkRenderingInfo info{};
	info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	info.colorAttachmentCount = static_cast<std::uint32_t>(color.size());
	info.layerCount = 1;
	info.pColorAttachments = color.empty() ? nullptr : color.data();
	info.pDepthAttachment = GetPointer(depth);
	info.pStencilAttachment = GetPointer(stencil);
	info.renderArea = area;

	return info;
}