#include "renderer/ImageTransitioner.h"
#include "renderer/CommandBuffer.h"

void ImageTransitioner::Transition(const CommandBuffer& cmdBuffer) const
{
	if (!CanTransition())
		return;

    VkImageMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memoryBarrier.oldLayout = oldLayout;
    memoryBarrier.newLayout = newLayout;
    memoryBarrier.srcAccessMask = srcAccess;
    memoryBarrier.dstAccessMask = dstAccess;
    memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.image = image;
    memoryBarrier.subresourceRange.aspectMask = aspectFlags;
    memoryBarrier.subresourceRange.baseMipLevel = 0;
    memoryBarrier.subresourceRange.levelCount = 1;
    memoryBarrier.subresourceRange.baseArrayLayer = 0;
    memoryBarrier.subresourceRange.layerCount = 1;

    cmdBuffer.PipelineBarrier(srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier); // try buffering this
}

void ImageTransitioner::Detransition(const CommandBuffer& cmdBuffer) const // every src is now dst and vice versa: calling transition and detransition after each other makes it so that the transition is undone
{
    if (!CanTransition())
        return;

    VkImageMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memoryBarrier.oldLayout = newLayout;
    memoryBarrier.newLayout = oldLayout;
    memoryBarrier.srcAccessMask = dstAccess;
    memoryBarrier.dstAccessMask = srcAccess;
    memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.image = image;
    memoryBarrier.subresourceRange.aspectMask = aspectFlags;
    memoryBarrier.subresourceRange.baseMipLevel = 0;
    memoryBarrier.subresourceRange.levelCount = 1;
    memoryBarrier.subresourceRange.baseArrayLayer = 0;
    memoryBarrier.subresourceRange.layerCount = 1;

    cmdBuffer.PipelineBarrier(dstStage, srcStage, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
}

bool ImageTransitioner::CanTransition() const
{
	bool dimensions = width != 0 && height != 0;
	bool hasImage = image != VK_NULL_HANDLE;
	bool layouts = oldLayout != INVALID_LAYOUT && newLayout != INVALID_LAYOUT;
	bool aspect = aspectFlags != 0;

	return dimensions && hasImage && layouts && aspect;
}