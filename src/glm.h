#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/easing.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <string>

inline std::string Vec3ToString(glm::vec3 vec3)
{
	return std::to_string(vec3.x) + std::to_string(vec3.y) + std::to_string(vec3.z);
}