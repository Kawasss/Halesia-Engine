module;

#include "renderer/VulkanAPIError.h"

module Renderer.CommandBuffer;

import <vulkan/vulkan.h>;

import Renderer.Vulkan;

bool CommandBuffer::IsValid() const
{
    return commandBuffer != VK_NULL_HANDLE;
}

void CommandBuffer::Reset(VkCommandBufferResetFlags flags) const
{
    ::vkResetCommandBuffer(commandBuffer, flags);
}

void CommandBuffer::Begin() const
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkResult result = ::vkBeginCommandBuffer(commandBuffer, &beginInfo);
    CheckVulkanResult("Failed to begin the given command buffer", result);
}

void CommandBuffer::End() const
{
    VkResult result = ::vkEndCommandBuffer(commandBuffer);
    CheckVulkanResult("Failed to record / end the command buffer", result);
}

void CommandBuffer::SetViewport(const VkViewport& viewport) const
{
    ::vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
}

void CommandBuffer::SetScissor(const VkRect2D& scissor) const
{
    ::vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void CommandBuffer::SetViewport(uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) const
{
    ::vkCmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports);
}

void CommandBuffer::SetScissor(uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) const
{
    ::vkCmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
}

void CommandBuffer::BeginRenderPass(const VkRenderPassBeginInfo& renderPassBegin, VkSubpassContents contents) const
{
    ::vkCmdBeginRenderPass(commandBuffer, &renderPassBegin, contents);
}

void CommandBuffer::EndRenderPass() const
{
    ::vkCmdEndRenderPass(commandBuffer);
}

void CommandBuffer::PushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) const
{
    ::vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
}

void CommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const
{
    ::vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBuffer::BeginRendering(const VkRenderingInfo& renderingInfo) const
{
    ::vkCmdBeginRendering(commandBuffer, &renderingInfo);
}

void CommandBuffer::EndRendering() const
{
    ::vkCmdEndRendering(commandBuffer);
}

void CommandBuffer::BeginDebugUtilsLabel(const char* pLabelName) const
{
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = pLabelName;
    
    ::vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);
}

void CommandBuffer::EndDebugUtilsLabel() const
{
    ::vkCmdEndDebugUtilsLabelEXT(commandBuffer);
}

void CommandBuffer::WriteTimestamp(VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) const
{
    ::vkCmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
}

void CommandBuffer::ResetQueryPool(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) const
{
    ::vkCmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
}

void CommandBuffer::BindPipeline(VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) const
{
    ::vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}

void CommandBuffer::BindDescriptorSets(VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) const
{
    ::vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

void CommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const
{
    ::vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) const
{
    ::vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBuffer::BindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) const
{
    ::vkCmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
}

void CommandBuffer::BindVertexBuffer(VkBuffer buffer, VkDeviceSize offset) const
{
    ::vkCmdBindVertexBuffers(commandBuffer, 0, 1, &buffer, &offset);
}

void CommandBuffer::BindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) const
{
    ::vkCmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
}

void CommandBuffer::PipelineBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) const
{
    ::vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

void CommandBuffer::MemoryBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers) const
{
    ::vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, 0, nullptr, 0, nullptr);
}

void CommandBuffer::BufferMemoryBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers) const
{
    ::vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, 0, nullptr, bufferMemoryBarrierCount, pBufferMemoryBarriers, 0, nullptr);
}

void CommandBuffer::ImageMemoryBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) const
{
    ::vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, 0, nullptr, 0, nullptr, imageMemoryBarrierCount, pImageMemoryBarriers);
}

void CommandBuffer::TraceRays(const VkStridedDeviceAddressRegionKHR& raygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR& missShaderBindingTable, const VkStridedDeviceAddressRegionKHR& hitShaderBindingTable, const VkStridedDeviceAddressRegionKHR& callableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) const
{
    ::vkCmdTraceRaysKHR(commandBuffer, &raygenShaderBindingTable, &missShaderBindingTable, &hitShaderBindingTable, &callableShaderBindingTable, width, height, depth);
}

void CommandBuffer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) const
{
    ::vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}

void CommandBuffer::CopyImage(VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) const
{
    ::vkCmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

void CommandBuffer::CopyBufferToImage(VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) const
{
    ::vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}

void CommandBuffer::FillBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) const
{
    ::vkCmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
}

void CommandBuffer::SetCullMode(VkCullModeFlags cullMode) const
{
    ::vkCmdSetCullMode(commandBuffer, cullMode);
}

void CommandBuffer::SetCheckpoint(const void* pCheckpointMarker) const
{
    ::vkCmdSetCheckpointNV(commandBuffer, pCheckpointMarker);
}