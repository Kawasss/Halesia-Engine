#pragma once
#include "glm.h"

class Transform
{
public:
	Transform() = default;
	Transform(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale, glm::vec3 extents, glm::vec3 center);

	glm::mat4 GetModelMatrix();
	glm::vec3 GetRight();
	glm::vec3 GetUp();
	glm::vec3 GetBackward();
	glm::vec3 GetForward();
	glm::vec3 GetGlobalScale();

	glm::vec3 position, scale, rotation;

private:
	glm::mat4 model;
	glm::vec3 extents, center; //should be moved to bounding box
};