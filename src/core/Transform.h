 #pragma once
#include "glm.h"

class Transform
{
public:
	Transform() = default;
	Transform(glm::vec3 translation, glm::quat rotation, glm::vec3 scale);

	glm::mat4 GetModelMatrix() const; // does NOT calculate the matrix, call CalculateModelMatrix for that
	glm::vec3 GetRight() const;
	glm::vec3 GetUp() const;
	glm::vec3 GetBackward() const;
	glm::vec3 GetForward() const;
	glm::vec3 GetGlobalScale() const;
	glm::vec3 GetGlobalPosition() const;

	float GetPitch() const;
	float GetYaw() const;

	void CalculateModelMatrix();

	glm::vec3 position = glm::vec3(0), scale = glm::vec3(1);
	glm::quat rotation;
	Transform* parent = nullptr;

private:
	glm::mat4 model = glm::identity<glm::mat4>();
};