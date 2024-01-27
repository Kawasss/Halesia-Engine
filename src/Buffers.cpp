#include <vulkan/vulkan.h>
#include <vector>
#include <iostream>
#include "renderer/Buffers.h"
#include "Vertex.h"

VertexBuffer::VertexBuffer(const std::vector<Vertex> vertices)
{
	this->logicalDevice = Vulkan::GetContext().logicalDevice;
	this->size = vertices.size();
	GenerateBuffer<Vertex>(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vertices);
}

IndexBuffer::IndexBuffer(const std::vector<uint16_t> indices)
{
	this->logicalDevice = Vulkan::GetContext().logicalDevice;
	GenerateBuffer<uint16_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, indices);
}