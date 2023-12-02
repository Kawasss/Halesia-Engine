#include "Camera.h"
#include "system/Input.h"
#include <iostream>
#include <algorithm>
#include "system/Window.h"
#include "Console.h"

void Camera::Update(Win32Window* window, float delta)
{
	DefaultUpdate(window, delta);
}

void Camera::DefaultUpdate(Win32Window* window, float delta)
{
	if (Console::isOpen)
		return;

	if (Input::IsKeyPressed(VirtualKey::W))
		position += front * (cameraSpeed * delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::S))
		position -= front * (cameraSpeed * delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::A))
		position -= right * (cameraSpeed * delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::D))
		position += right * (cameraSpeed * delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::Space))
		position += up * (cameraSpeed * delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::LeftShift))
		position -= up * (cameraSpeed * delta * 0.001f);

	int newPosX, newPosY;
	window->GetRelativeCursorPosition(newPosX, newPosY);
	
	SetYaw(glm::degrees(yaw) + newPosX * delta * SENSITIVITY);
	SetPitch(glm::degrees(pitch) - newPosY * delta * SENSITIVITY);
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
	
	UpdateUpAndRightVectors();
}

void Camera::UpdateUpAndRightVectors()
{
	right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
	up = glm::normalize(glm::cross(right, front));
}

// orbit camera

void OrbitCamera::Update(Win32Window* window, float delta)
{
	if (Console::isOpen)
		return;

	if (Input::IsKeyPressed(VirtualKey::W))
		pivot += front * (cameraSpeed * delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::S))
		pivot -= front * (cameraSpeed * delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::A))
		pivot -= right * (cameraSpeed * delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::D))
		pivot += right * (cameraSpeed * delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::Space))
		pivot += up * (cameraSpeed * delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::LeftShift))
		pivot -= up * (cameraSpeed * delta * 0.001f);

	int x, y;
	window->GetRelativeCursorPosition(x, y);

	sumX += x * delta;
	sumY += y * delta;

	radius += window->GetWheelRotation() * delta * 0.001f;
	if (radius < 0.1f) radius = 0.1f;

	float phi = sumX * 2 * glm::pi<float>() / window->GetWidth();
	float theta = std::clamp(sumY * glm::pi<float>() / window->GetHeight(), -0.49f * glm::pi<float>(), 0.49f * glm::pi<float>());

	position.x = radius * (cos(phi) * cos(theta));
	position.y = radius * sin(theta);
	position.z = radius * (cos(theta) * sin(phi));
	position += pivot;
	position.y = std::clamp(position.y, -pivot.y, pivot.y);

	front = glm::normalize(pivot - position);

	right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
	up = glm::normalize(glm::cross(right, front));
}