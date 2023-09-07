#include "Transform.h"

Transform::Transform(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec3 extents, glm::vec3 center)
{
	this->position = position;
	this->rotation = rotation;
	this->scale = scale;
	this->extents = extents;
	this->center = center;
}

glm::mat4 Transform::GetModelMatrix()
{
	glm::mat4 rotationModel = glm::rotate(glm::mat4(1), glm::radians(rotation.x), glm::vec3(1, 0, 0)) * glm::rotate(glm::mat4(1), glm::radians(rotation.y), glm::vec3(0, 1, 0)) * glm::rotate(glm::mat4(1), glm::radians(rotation.z), glm::vec3(0, 0, 1));
	glm::mat4 scaleModel = glm::scale(glm::identity<glm::mat4>(), scale);
	glm::mat4 translationModel = glm::translate(glm::identity<glm::mat4>(), position);
	model = translationModel * rotationModel * scaleModel;
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
	return glm::vec3((float)GetRight().length(), (float)GetUp().length(), (float)GetBackward().length());
}