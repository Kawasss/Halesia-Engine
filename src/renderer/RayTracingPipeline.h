#pragma once
#include <string>
#include <vector>
#include <array>
#include <map>

#include "Pipeline.h"

class ShaderGroupReflector;

class RayTracingPipeline : public Pipeline
{
public:
	RayTracingPipeline(const std::string& rgen, const std::string& rchit, const std::string& rmiss);

private:
	void CreatePipeline(const std::vector<std::vector<char>>& shaders);

	VkStridedDeviceAddressRegionKHR rchitShaderBinding{};
	VkStridedDeviceAddressRegionKHR rgenShaderBinding{};
	VkStridedDeviceAddressRegionKHR rmissShaderBinding{};
	VkStridedDeviceAddressRegionKHR callableShaderBinding{};

	std::map<std::string, BindingLayout> nameToLayout;
};