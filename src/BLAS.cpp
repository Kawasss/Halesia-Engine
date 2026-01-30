module Renderer.BLAS;

import std;

import <vulkan/vulkan.h>;

import Renderer.Vertex;
import Renderer.Vulkan;
import Renderer;

static std::uint32_t GetFaceCount(StorageBuffer<std::uint32_t>::Memory indexMemory)
{
	return static_cast<std::uint32_t>(Renderer::g_indexBuffer.GetItemCount(indexMemory) / 3);
}

static VkAccelerationStructureGeometryTrianglesDataKHR GetTrianglesData(StorageBuffer<Vertex>::Memory vertexMemory, StorageBuffer<std::uint32_t>::Memory indexMemory)
{
	VkAccelerationStructureGeometryTrianglesDataKHR data{};
	data.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	data.vertexData = { Renderer::g_vertexBuffer.GetDeviceAddressOffset(vertexMemory) };
	data.indexData  = { Renderer::g_indexBuffer.GetDeviceAddressOffset(indexMemory)   };
	data.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	data.vertexStride = sizeof(Vertex);
	data.maxVertex = static_cast<std::uint32_t>(Renderer::g_vertexBuffer.GetItemCount(vertexMemory));
	data.indexType = VK_INDEX_TYPE_UINT32;

	data.transformData = { 0 };

	return data;
}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(StorageBuffer<Vertex>::Memory vertexMemory, StorageBuffer<std::uint32_t>::Memory indexMemory)
{
	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.triangles = ::GetTrianglesData(vertexMemory, indexMemory);

	std::uint32_t faceCount = ::GetFaceCount(indexMemory);

	CreateAS(&geometry, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, faceCount * 10); // * 10 ????
	BuildAS(&geometry, faceCount, VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR);
}

BottomLevelAccelerationStructure* BottomLevelAccelerationStructure::Create(StorageBuffer<Vertex>::Memory vertexMemory, StorageBuffer<std::uint32_t>::Memory indexMemory)
{
	BottomLevelAccelerationStructure* BLAS = new BottomLevelAccelerationStructure(vertexMemory, indexMemory);
	return BLAS;
}

void BottomLevelAccelerationStructure::RebuildGeometry(VkCommandBuffer commandBuffer, StorageBuffer<Vertex>::Memory vertexMemory, StorageBuffer<std::uint32_t>::Memory indexMemory)
{
	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.triangles = ::GetTrianglesData(vertexMemory, indexMemory);

	BuildAS(&geometry, ::GetFaceCount(indexMemory), VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, commandBuffer);
}