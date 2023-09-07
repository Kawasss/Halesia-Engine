#include "Camera.h"
#include "system/Input.h"

void Camera::Update(Win32Window* window, float delta)
{
	DefaultUpdate(window, delta);
}

void Camera::DefaultUpdate(Win32Window* window, float delta)
{
	if (Input::IsKeyPressed(VirtualKey::W))
		position += front * (cameraSpeed * delta * 0.1f);
	if (Input::IsKeyPressed(VirtualKey::S))
		position -= front * (cameraSpeed * delta * 0.1f);
	if (Input::IsKeyPressed(VirtualKey::A))
		position -= right * (cameraSpeed * delta * 0.1f);
	if (Input::IsKeyPressed(VirtualKey::D))
		position += right * (cameraSpeed * delta * 0.1f);
	if (Input::IsKeyPressed(VirtualKey::Space))
		position += up * (cameraSpeed * delta * 0.1f);
	if (Input::IsKeyPressed(VirtualKey::LeftControl))
		position -= up * (cameraSpeed * delta * 0.1f);

	int newPosX, newPosY;
	window->GetRelativeCursorPosition(newPosX, newPosY);

	SetYaw(glm::degrees(yaw) + newPosX * SENSITIVITY);
	SetPitch(glm::degrees(pitch) - newPosY * SENSITIVITY);
}

Camera::Camera(glm::vec3 position, float aspectRatio)
{
	this->position = position;
	this->aspectRatio = aspectRatio;
	UpdateVectors();
}

void Camera::SetNearPlane(float value)
{
	if (value <= 0)
		return;
	nearPlane = value;
}

void Camera::SetFarPlane(float value)
{
	if (value <= 0)
		return;
	farPlane = value;
}

void Camera::SetPitch(float value)
{
	float angle = glm::clamp(value, -89.0f, 89.0f);
	pitch = glm::radians(angle);
	UpdateVectors();
}

void Camera::SetYaw(float value)
{
	yaw = glm::radians(value);
	UpdateVectors();
}

void Camera::SetFOV(float value)
{
	float angle = glm::clamp(value, 1.0f, 90.0f);
	fov = glm::radians(angle);
}

glm::mat4 Camera::GetProjectionMatrix()
{
	glm::mat4 projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);
	projection[1][1] *= -1;
	return projection;
}

glm::mat4 Camera::GetOrthoProjectionMatrix()
{
	glm::mat4 projection = glm::ortho(-1, 1, -1, 1); //unsure of parameters
	projection[1][1] *= -1;
	return projection;
}

glm::mat4 Camera::GetViewMatrix()
{
	return glm::lookAt(position, position + front, up);
}

void Camera::UpdateVectors()
{
	front.x = glm::cos(pitch) * glm::cos(yaw);
	front.y = glm::sin(pitch);
	front.z = glm::cos(pitch) * glm::sin(yaw);

	front = glm::normalize(front);
	
	right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
	up = glm::normalize(glm::cross(right, front));
}