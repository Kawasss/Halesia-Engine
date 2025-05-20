#include "core/Camera.h"
#include "core/Console.h"

#include "system/Input.h"
#include "system/Window.h"

void Camera::Update(Window* window, float delta)
{
	DefaultUpdate(window, delta);
}

void Camera::DefaultUpdate(Window* window, float delta)
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

glm::mat4 Camera::GetProjectionMatrix() const
{
	glm::mat4 projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);
	projection[1][1] *= -1;
	return projection;
}

glm::mat4 Camera::GetOrthoProjectionMatrix(unsigned int width, unsigned int height) const
{
	float halfWidth  = width * 0.5f;
	float halfHeight = height * 0.5f;

	glm::mat4 projection = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, -farPlane, farPlane); //unsure of parameters
	projection[1][1] *= -1;
	return projection;
}

glm::mat4 Camera::GetViewMatrix() const
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

void Camera::UpdateVelocityMatrices()
{
	prevView = GetViewMatrix();
	prevProj = GetProjectionMatrix();
	prevPos = position;
}

glm::vec2 Camera::GetMotionVector() const // deprecated
{
	glm::vec4 curr = GetProjectionMatrix() * GetViewMatrix() * glm::vec4(position, 1);
	glm::vec4 prev = prevProj * prevView * glm::vec4(position, 1);

	glm::vec2 diff = glm::vec2(curr) / (curr.w + 0.001f) - glm::vec2(prev) / (prev.w + 0.001f);
	if (diff != diff) // check for NaN
		diff = glm::vec2(0);

	return diff;
}

glm::mat4 Camera::GetPreviousViewMatrix() const
{
	return prevView;
}

glm::mat4 Camera::GetPreviousProjectionMatrix() const
{
	return prevProj;
}

// orbit camera

void OrbitCamera::Update(Window* window, float delta)
{
	if (Console::isOpen)
		return;

	radius += window->GetWheelRotation() * delta * 0.001f;
	if (radius < 0.1f) radius = 0.1f;

	float phi = sumX * 2 * glm::pi<float>() / window->GetWidth();
	float theta = glm::clamp(sumY * glm::pi<float>() / window->GetHeight(), -0.49f * glm::pi<float>(), 0.49f * glm::pi<float>());

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