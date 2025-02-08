#pragma once
#include <string>
#include <vector>

#include "Pipeline.h"
#include "Buffer.h"

class ShaderGroupReflector;

class RayTracingPipeline : public Pipeline
{
public:
	RayTracingPipeline(const std::string& rgen, const std::string& rchit, const std::string& rmiss);

	void Execute(const CommandBuffer& cmdBuffer, uint32_t width, uint32_t height, uint32_t depth) const;

private:
	void CreatePipeline(const ShaderGroupReflector& reflector, const std::vector<std::vector<char>>& shaders);
	void CreateShaderBindingTable();

	VkStridedDeviceAddressRegionKHR rchitShaderBinding{};
	VkStridedDeviceAddressRegionKHR rgenShaderBinding{};
	VkStridedDeviceAddressRegionKHR rmissShaderBinding{};
	VkStridedDeviceAddressRegionKHR callableShaderBinding{};

	Buffer shaderBindingTableBuffer;
};