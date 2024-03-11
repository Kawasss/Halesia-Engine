#pragma once
#include "Swapchain.h"
#include "PhysicalDevice.h"
#include "Vulkan.h"
#include <cstdint>

enum PipelineFlags : uint16_t
{
	PIPELINE_FLAG_NONE = 0,
	PIPELINE_FLAG_NO_DEPTH = 1 << 0,
	PIPELINE_FLAG_NO_VERTEX = 1 << 1,
	PIPELINE_FLAG_CLEAR_ON_LOAD = 1 << 2,
	PIPELINE_FLAG_SRGB_ATTACHMENT = 1 << 3,
	PIPELINE_FLAG_NO_BLEND = 1 << 4,
	PIPELINE_FLAG_CULL_BACK = 1 << 5,
	PIPELINE_FLAG_FRONT_CCW = 1 << 6,
}; // also one with polygon mode

namespace Creator
{
	inline VkRenderPass CreateRenderPass(PhysicalDevice physicalDevice, Swapchain* swapchain, PipelineFlags flags, uint32_t attachmentCount)
	{
		std::vector<VkAttachmentDescription> attachments(attachmentCount);
		std::vector<VkAttachmentReference> colorReferences(attachmentCount);

		for (uint32_t i = 0; i < attachmentCount; i++)
		{
			attachments[i].format = swapchain->format;
			attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[i].loadOp = flags & PIPELINE_FLAG_CLEAR_ON_LOAD ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[i].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			colorReferences[i].attachment = i;
			colorReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		if (!(flags & PIPELINE_FLAG_NO_DEPTH))
		{
			VkAttachmentDescription depthAttachment{};
			depthAttachment.format = physicalDevice.GetDepthFormat();
			depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			attachments.push_back(depthAttachment);
		}

		VkAttachmentReference depthReference{};
		depthReference.attachment = 1;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = colorReferences.data();
		subpass.pDepthStencilAttachment = flags & PIPELINE_FLAG_NO_DEPTH ? nullptr : &depthReference;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcAccessMask = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VkRenderPass renderPass = VK_NULL_HANDLE;
		VkResult result = vkCreateRenderPass(Vulkan::GetContext().logicalDevice, &renderPassInfo, nullptr, &renderPass);
		CheckVulkanResult("Failed to create a render pass", result, vkCreateRenderPass);

		return renderPass;
	}
}