module;

#include "Buffer.h"

export module Renderer.AccelerationStructure;

import <vulkan/vulkan.h>;

import std;

export class AccelerationStructure // or AS for short
{
public:
	VkDeviceAddress GetAccelerationStructureAddress() { return ASAddress; }
	VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;

	~AccelerationStructure();

protected:
	void CreateAS(const VkAccelerationStructureGeometryKHR* pGeometry, VkAccelerationStructureTypeKHR type, uint32_t maxPrimitiveCount);
	void BuildAS(const VkAccelerationStructureGeometryKHR* pGeometry, uint32_t primitiveCount, VkBuildAccelerationStructureModeKHR mode, VkCommandBuffer externalCommandBuffer = VK_NULL_HANDLE);

	Buffer ASBuffer;
	Buffer scratchBuffer;

	VkDeviceSize size = 0;
	VkDeviceSize buildSize = 0;
	VkDeviceSize UpdateSize = 0;

	VkDeviceAddress ASAddress = 0;

private:
	VkAccelerationStructureTypeKHR type = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
};

