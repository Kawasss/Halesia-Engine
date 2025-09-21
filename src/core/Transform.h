 #pragma once
#include "glm.h"

class Transform
{
public:
	Transform() = default;
	Transform(glm::vec3 translation, glm::quat rotation, glm::vec3 scale);

	glm::mat4 GetModelMatrix() const; // does NOT calculate the matrix, call CalculateModelMatrix for that
	glm::vec3 GetRight();
	glm::vec3 GetUp();
	glm::vec3 GetBackward();
	glm::vec3 GetForward();
	glm::vec3 GetGlobalScale();
	glm::vec3 GetFullPosition();

	float GetPitch();
	float GetYaw();

	void CalculateModelMatrix();

	glm::vec3 position = glm::vec3(0), scale = glm::vec3(1);
	glm::quat rotation;
	Transform* parent = nullptr;

private:
	glm::mat4 model = glm::identity<glm::mat4>();
};