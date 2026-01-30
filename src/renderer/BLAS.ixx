export module Renderer.BLAS;

import <vulkan/vulkan.h>;

import std;

import Renderer.AccelerationStructure;
import Renderer.StorageBuffer;
import Renderer.Vertex;

export class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
	BottomLevelAccelerationStructure(StorageBuffer<Vertex>::Memory vertexMemory, StorageBuffer<std::uint32_t>::Memory indexMemory);

	static BottomLevelAccelerationStructure* Create(StorageBuffer<Vertex>::Memory vertexMemory, StorageBuffer<std::uint32_t>::Memory indexMemory);
	void RebuildGeometry(VkCommandBuffer commandBuffer, StorageBuffer<Vertex>::Memory vertexMemory, StorageBuffer<std::uint32_t>::Memory indexMemory);
};