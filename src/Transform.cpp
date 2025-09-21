#include "core/Transform.h"

Transform::Transform(glm::vec3 position, glm::quat rotation, glm::vec3 scale)
{
	this->position = position;
	this->rotation = rotation;
	this->scale = scale;
}

void Transform::CalculateModelMatrix()
{
	glm::mat4 scaleModel = glm::scale(scale);
	glm::mat4 translationModel = glm::translate(position);

	glm::mat4 local = translationModel * glm::toMat4(rotation) * scaleModel;

	model = parent == nullptr ? local : parent->GetModelMatrix() * local;
}

glm::mat4 Transform::GetModelMatrix() const
{
	return model;
}

glm::vec3 Transform::GetRight()
{
	return glm::vec3(model[0][0], model[0][1], model[0][2]);
}

glm::vec3 Transform::GetUp()
{
	return glm::vec3(model[1][0], model[1][1], model[1][2]);
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

glm::vec3 Transform::GetGlobalPosition()
{
	return glm::vec3(model[3][0], model[3][1], model[3][2]);
}

float Transform::GetYaw()
{
	return atan2(GetForward()[1], GetForward()[0]);
}

float Transform::GetPitch()
{
	return -asin(GetForward()[2]);//atan2(-model[3][1], sqrt(model[3][2]*model[3][2] + model[3][3]*model[3][3]));
}