module Renderer.BLAS;

import <vulkan/vulkan.h>;

static VkAccelerationStructureGeometryTrianglesDataKHR GetTrianglesData(const Mesh& mesh)
{
	VkDeviceAddress vertexBufferAddress = Vulkan::GetDeviceAddress(Renderer::g_vertexBuffer.GetBufferHandle());
	VkDeviceAddress indexBufferAddress = Vulkan::GetDeviceAddress(Renderer::g_indexBuffer.GetBufferHandle());

	VkAccelerationStructureGeometryTrianglesDataKHR data{};
	data.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;

	data.vertexData = { vertexBufferAddress + Renderer::g_vertexBuffer.GetMemoryOffset(mesh.vertexMemory) };
	data.indexData = { indexBufferAddress + Renderer::g_indexBuffer.GetMemoryOffset(mesh.indexMemory) };

	data.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	data.vertexStride = sizeof(Vertex);
	data.maxVertex = static_cast<uint32_t>(mesh.vertices.size());
	data.indexType = VK_INDEX_TYPE_UINT32;

	data.transformData = { 0 };

	return data;
}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(const Mesh& mesh)
{
	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.triangles = GetTrianglesData(mesh);

	CreateAS(&geometry, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, mesh.faceCount * 10);
	BuildAS(&geometry, mesh.faceCount, VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR);
}

BottomLevelAccelerationStructure* BottomLevelAccelerationStructure::Create(const Mesh& mesh)
{
	BottomLevelAccelerationStructure* BLAS = new BottomLevelAccelerationStructure(mesh);
	return BLAS;
}

void BottomLevelAccelerationStructure::RebuildGeometry(VkCommandBuffer commandBuffer, const Mesh& mesh)
{
	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.triangles = GetTrianglesData(mesh);

	BuildAS(&geometry, mesh.faceCount, VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, commandBuffer);
}