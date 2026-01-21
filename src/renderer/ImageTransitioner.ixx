export module Renderer.ImageTransitioner;

import <vulkan/vulkan.h>;

import std;

import Renderer.CommandBuffer;

export class ImageTransitioner
{
public:
	static constexpr VkImageLayout INVALID_LAYOUT = VK_IMAGE_LAYOUT_MAX_ENUM;

	VkImage image = VK_NULL_HANDLE;

	std::uint32_t width = 0;
	std::uint32_t height = 0;

	VkImageLayout oldLayout = INVALID_LAYOUT;
	VkImageLayout newLayout = INVALID_LAYOUT;

	VkAccessFlags srcAccess = 0;
	VkAccessFlags dstAccess = 0;

	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

	VkImageAspectFlags aspectFlags = 0;

	ImageTransitioner() = default;
	ImageTransitioner(VkImage image) : image(image) {}

	void Transition(const CommandBuffer& cmdBuffer) const;
	void Detransition(const CommandBuffer& cmdBuffer) const;

private:
	bool CanTransition() const; // are all values usable ?
};