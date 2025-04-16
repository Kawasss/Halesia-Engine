#include "core/Transform.h"

Transform::Transform(glm::vec3 position, glm::quat rotation, glm::vec3 scale, glm::vec3 extents, glm::vec3 center)
{
	this->position = position;
	this->rotation = rotation;
	this->scale = scale;
	this->extents = extents;
	this->center = center;
}

glm::mat4 Transform::GetModelMatrix()
{
	if (!localRotationChanged)
		localRotationModel = glm::toMat4(rotation);
	glm::mat4 scaleModel = glm::scale(glm::identity<glm::mat4>(), scale);
	glm::mat4 translationModel = glm::translate(glm::identity<glm::mat4>(), position);
	model = translationModel * localRotationModel * scaleModel;
	localRotationChanged = false;
	return parent == nullptr ? model : parent->GetModelMatrix() * model;
}

glm::vec3 Transform::GetRight()
{
	return glm::vec3(model[0][0], model[0][1], model[0][2]);
}

glm::vec3 Transform::GetUp()
{
	return glm::vec3(model[1][0], model[1][1], model[1][2]);
}

glm::vec3 Transform::GetLocalUp()
{
	return glm::vec3(localRotationModel[1][0], localRotationModel[1][1], localRotationModel[1][2]);
}

glm::vec3 Transform::GetBackward()
{
	return glm::vec3(model[2][0], model[2][1], model[2][2]);
}
glm::vec3 Transform::GetForward()
{
	return -glm::vec3(model[2][0], model[2][1], model[2][2]);
}

glm::vec3 Transform::GetGlobalScale()
{
	return glm::vec3(glm::length(GetRight()), glm::length(GetUp()), glm::length(GetBackward()));
}

float Transform::GetYaw()
{
	return atan2(GetForward()[1], GetForward()[0]);
}

float Transform::GetPitch()
{
	return -asin(GetForward()[2]);//atan2(-model[3][1], sqrt(model[3][2]*model[3][2] + model[3][3]*model[3][3]));
}

glm::mat4 Transform::GetLocalRotation()
{
	return localRotationModel;
}

void Transform::SetLocalRotation(glm::mat4 newLocalRotation)
{
	localRotationModel = newLocalRotation;
	localRotationChanged = true;
}
#include <iostream>
glm::vec2 Transform::GetMotionVector(glm::mat4 projection, glm::mat4 view, glm::mat4 model)
{
	glm::vec4 currentClipSpace = projection * view * model * glm::vec4(position, 1);

	glm::vec2 difference = glm::vec2(currentClipSpace) / (currentClipSpace.w + 0.001f) - glm::vec2(previousClipPosition) / (previousClipPosition.w + 0.001f);

	if (difference != difference) // if the player is at 0, 0 ret will be NaN, according to IEEE NaN cannot be equal to NaN so this checks for that
		difference = glm::vec2(0);
	std::cout << difference.x << ", " << difference.y << '\n';
	previousClipPosition = currentClipSpace;
	return difference;
}