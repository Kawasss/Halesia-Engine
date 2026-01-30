export module Renderer.BLAS;

import <vulkan/vulkan.h>;

import Renderer.AccelerationStructure;
import Renderer.Mesh;

export class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
	BottomLevelAccelerationStructure(const Mesh& mesh);

	static BottomLevelAccelerationStructure* Create(const Mesh& mesh);
	void RebuildGeometry(VkCommandBuffer commandBuffer, const Mesh& mesh);
};