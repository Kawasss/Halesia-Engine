#pragma once
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
#include <sstream>
#include <iomanip>

template<typename T> inline std::string ToHexadecimalString(T number) // no better place to put this
{
	std::stringstream stream;
	stream << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex << number;
	return stream.str();
}

inline std::string Vec3ToString(glm::vec3 vec3)
{
	std::stringstream stream;
	stream << vec3.x << ", " << vec3.y << ", " << vec3.z;
	return stream.str();
}