module;

#include "renderer/Mesh.h"
#include "renderer/VulkanAPIError.h"

#include "system/CriticalSection.h"

module Renderer.AccelerationStructure;

import <vulkan/vulkan.h>;

import std;

import Core.MeshObject;

import Renderer.VulkanGarbageManager;
import Renderer.Vulkan;
import Renderer;

constexpr VkBufferUsageFlags ACCELERATION_STRUCTURE_BUFFER_BITS = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
constexpr VkBufferUsageFlags SCRATCH_BUFFER_BITS = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;



void AccelerationStructure::CreateAS(const VkAccelerationStructureGeometryKHR* pGeometry, VkAccelerationStructureTypeKHR type, uint32_t maxPrimitiveCount)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	this->type = type;

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
	buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR;
	buildGeometryInfo.type = type;
	buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	buildGeometryInfo.geometryCount = 1;
	buildGeometryInfo.pGeometries = pGeometry;
	buildGeometryInfo.scratchData = { 0 };

	VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
	buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	vkGetAccelerationStructureBuildSizesKHR(ctx.logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &maxPrimitiveCount, &buildSizesInfo);

	size       = buildSizesInfo.accelerationStructureSize;
	buildSize  = buildSizesInfo.buildScratchSize;
	UpdateSize = buildSizesInfo.updateScratchSize;

	ASBuffer.Init(size, ACCELERATION_STRUCTURE_BUFFER_BITS, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkAccelerationStructureCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.type = type;
	createInfo.size = size;
	createInfo.buffer = ASBuffer.Get();
	createInfo.offset = 0;

	VkResult result = vkCreateAccelerationStructureKHR(ctx.logicalDevice, &createInfo, nullptr, &accelerationStructure);
	CheckVulkanResult("Failed to create an acceleration structure", result);

	if (type == VkAccelerationStructureTypeKHR::VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
		Vulkan::SetDebugName(accelerationStructure, "top-level");
	else
		Vulkan::SetDebugName(accelerationStructure, "bottom-level");

	ASAddress = Vulkan::GetDeviceAddress(accelerationStructure);

	scratchBuffer.Init(buildSize, SCRATCH_BUFFER_BITS, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void AccelerationStructure::BuildAS(const VkAccelerationStructureGeometryKHR* pGeometry, uint32_t primitiveCount, VkBuildAccelerationStructureModeKHR mode, VkCommandBuffer externalCommandBuffer)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
	buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR;
	buildGeometryInfo.mode = mode;
	buildGeometryInfo.type = type;
	buildGeometryInfo.srcAccelerationStructure = accelerationStructure;
	buildGeometryInfo.dstAccelerationStructure = accelerationStructure;
	buildGeometryInfo.geometryCount = 1;
	buildGeometryInfo.pGeometries = pGeometry;
	buildGeometryInfo.scratchData = { Vulkan::GetDeviceAddress(scratchBuffer.Get()) };

	VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
	buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	vkGetAccelerationStructureBuildSizesKHR(ctx.logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &primitiveCount, &buildSizesInfo);

	if (buildSize < buildSizesInfo.buildScratchSize)
		return; // build request too big

	VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
	buildRangeInfo.primitiveCount = primitiveCount;
	const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;

	if (externalCommandBuffer == VK_NULL_HANDLE)
	{
		Vulkan::ExecuteSingleTimeCommands([&](const CommandBuffer& cmdBuffer)
			{
				vkCmdBuildAccelerationStructuresKHR(cmdBuffer.Get(), 1, &buildGeometryInfo, &pBuildRangeInfo);
			}
		);
	}
	else // this option is faster for runtime building since it doesn't wait for the queue to go idle (which can be a long time)
	{
		VkMemoryBarrier memoryBarrier{};
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

		vkCmdBuildAccelerationStructuresKHR(externalCommandBuffer, 1, &buildGeometryInfo, &pBuildRangeInfo);
		vkCmdPipelineBarrier(externalCommandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	}
}

AccelerationStructure::~AccelerationStructure()
{
	vgm::Delete(accelerationStructure);
}

