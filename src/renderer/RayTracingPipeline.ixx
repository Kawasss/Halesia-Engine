module;

#include "Buffer.h"
#include "CommandBuffer.h"

export module Renderer.RayTracingPipeline;

import <vulkan/vulkan.h>;

import std;

import Renderer.ShaderReflector;
import Renderer.Pipeline;

export class RayTracingPipeline : public Pipeline
{
public:
	RayTracingPipeline(const std::string& rgen, const std::string& rchit, const std::string& rmiss);

	void Execute(const CommandBuffer& cmdBuffer, std::uint32_t width, std::uint32_t height, std::uint32_t depth) const;

private:
	void CreatePipeline(const ShaderGroupReflector& reflector, const std::span<std::span<char>>& shaders);
	void CreateShaderBindingTable();

	VkStridedDeviceAddressRegionKHR rchitShaderBinding{};
	VkStridedDeviceAddressRegionKHR rgenShaderBinding{};
	VkStridedDeviceAddressRegionKHR rmissShaderBinding{};
	VkStridedDeviceAddressRegionKHR callableShaderBinding{};

	Buffer shaderBindingTableBuffer;
};