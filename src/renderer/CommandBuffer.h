#pragma once
#include <vulkan/vulkan.h>
#undef MemoryBarrier // weird winnt preprocessor stuff

class CommandBuffer
{
public:
	CommandBuffer() = default;
	CommandBuffer(VkCommandBuffer cmdBuffer) : commandBuffer(cmdBuffer) {}

	bool IsValid() const;

	void Reset(VkCommandBufferResetFlags flags = 0) const;

	void Begin() const;
	void End() const;

	void SetViewport(const VkViewport& viewport) const;
	void SetScissor(const VkRect2D& scissor) const;

	void SetViewport(uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) const;
	void SetScissor(uint32_t firstScissor, uint32_t scissorCount,  const VkRect2D* pScissors) const;

	void BeginRenderPass(const VkRenderPassBeginInfo& renderPassBegin, VkSubpassContents contents) const;
	void EndRenderPass() const;

	void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) const;

	void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const;
	void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) const;
	void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const;

	void BeginRendering(const VkRenderingInfo& renderingInfo) const;
	void EndRendering() const;

	void BeginDebugUtilsLabel(const char* pLabelName) const; // color not supported
	void EndDebugUtilsLabel() const;

	void WriteTimestamp(VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) const;
	void ResetQueryPool(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) const;

	void BindPipeline(VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) const;
	void BindDescriptorSets(VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) const;

	void BindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) const;
	void BindVertexBuffer(VkBuffer buffer, VkDeviceSize offset) const;
	void BindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) const;

	void PipelineBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) const;
	void MemoryBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers) const;
	void BufferMemoryBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers) const;
	void ImageMemoryBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) const;

	void TraceRays(const VkStridedDeviceAddressRegionKHR& raygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR& missShaderBindingTable, const VkStridedDeviceAddressRegionKHR& hitShaderBindingTable, const VkStridedDeviceAddressRegionKHR& callableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) const;

	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) const;
	void CopyImage(VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) const;
	void CopyBufferToImage(VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) const;

	void FillBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) const;

	void SetCullMode(VkCullModeFlags cullMode) const;

	void SetCheckpoint(const void* pCheckpointMarker) const;

	VkCommandBuffer& Get() { return commandBuffer; }
	const VkCommandBuffer& Get() const { return commandBuffer; }

private:
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
};