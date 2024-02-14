#include <iostream>
#include <algorithm>

#include "core/Camera.h"
#include "core/Console.h"

#include "system/Input.h"
#include "system/Window.h"

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

glm::vec2 Camera::GetMotionVector()
{
	glm::vec4 NDC = GetProjectionMatrix() * GetViewMatrix() * glm::vec4(0, 0, 0, 1);
	NDC /= NDC.w;

	glm::vec2 current = glm::vec2((NDC.x + 1.0f) * 0.5f, (NDC.y + 1.0f) * 0.5f);
	glm::vec2 ret = current - prev2D;
	if (ret != ret) // if the player is at 0, 0 ret will be NaN, according to IEEE NaN cannot be equal to NaN so this checks for that
		ret = glm::vec2(0);
	prev2D = current;
	return ret;
}

// orbit camera

void OrbitCamera::Update(Win32Window* window, float delta)
{
	if (Console::isOpen)
		return;

	radius += window->GetWheelRotation() * delta * 0.001f;
	if (radius < 0.1f) radius = 0.1f;

	float phi = sumX * 2 * glm::pi<float>() / window->GetWidth();
	float theta = std::clamp(sumY * glm::pi<float>() / window->GetHeight(), -0.49f * glm::pi<float>(), 0.49f * glm::pi<float>());

	position.x = radius * (cos(phi) * cos(theta));
	position.y = radius * sin(theta);
	position.z = radius * (cos(theta) * sin(phi));
	position += pivot;

	front = glm::normalize(pivot - position);

	right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
	up = glm::normalize(glm::cross(right, front));

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

	if (!Input::IsKeyPressed(VirtualKey::MiddleMouseButton))
		return;

	int x, y;
	window->GetRelativeCursorPosition(x, y);

	sumX = sumX + x * delta;
	sumY = sumY + y * delta;
}