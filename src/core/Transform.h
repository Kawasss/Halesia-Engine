#pragma once
#include "glm.h"

class Transform
{
public:
	Transform() = default;
	Transform(glm::vec3 translation, glm::quat rotation, glm::vec3 scale);

	glm::mat4 GetModelMatrix();
	glm::mat4 GetLocalRotation();
	glm::vec3 GetRight();
	glm::vec3 GetUp();
	glm::vec3 GetLocalUp();
	glm::vec3 GetBackward();
	glm::vec3 GetForward();
	glm::vec3 GetGlobalScale();
	glm::vec2 GetMotionVector(glm::mat4 projection, glm::mat4 view, glm::mat4 model); // calculates the motion in vector mapped to a normalized vector

	float GetPitch();
	float GetYaw();

	void SetLocalRotation(glm::mat4 newLocalRotation);

	glm::vec3 position = glm::vec3(0), scale = glm::vec3(1);
	glm::quat rotation;
	Transform* parent = nullptr;

private:
	glm::vec4 previousClipPosition = glm::vec4(0.0000001f);

	glm::mat4 model = glm::identity<glm::mat4>(), localRotationModel = glm::identity<glm::mat4>();
	bool localRotationChanged = false;
};