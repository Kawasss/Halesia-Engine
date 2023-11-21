#pragma once
#include "glm.h"
#include "system/Window.h"

class Camera
{
public:
	Camera(glm::vec3 position = glm::vec3(0), float aspectRatio = monitorWidth / (float)monitorHeight);

	float cameraSpeed = 5;
	float pitch = 0, yaw = -(glm::pi<float>() / 2), fov = glm::pi<float>() / 2;

	glm::vec3 position	= glm::vec3(0);
	glm::vec3 up		= glm::vec3(0, 1, 0);
	glm::vec3 right		= glm::vec3(1, 0, 0);
	glm::vec3 front		= glm::vec3(1, 0, 0);

	void SetNearPlane(float newNearPlane);
	void SetFarPlane(float newFarPlane);

	void SetPitch(float newPitch);
	void SetYaw(float newYaw);
	void SetFOV(float newFov);

	virtual void Update(Win32Window* window, float delta);
	void DefaultUpdate(Win32Window* window, float delta);

	glm::mat4 GetProjectionMatrix();
	glm::mat4 GetOrthoProjectionMatrix();
	glm::mat4 GetViewMatrix();

	template<typename T> T GetScript() { return static_cast<T>(attachedScript); }

private:
	void* attachedScript = nullptr;
	void UpdateVectors();

	float nearPlane = 0.01f, farPlane = 1000, aspectRatio = monitorWidth / (float)monitorHeight;

protected:
	/// <summary>
	/// This function sets the script for the base class. This makes it so that GetScript can be used.
	/// </summary>
	/// <param name="script"></param>
	void SetScript(void* script) { attachedScript = script; }

	/// <summary>
	/// Updates the up and right vector based on the current front vector
	/// </summary>
	void UpdateUpAndRightVectors();

	const float SENSITIVITY = 0.1f;
};

class OrbitCamera : public Camera
{
public:
	glm::vec3 pivot = glm::vec3(0);
	float radius	= 2;

	OrbitCamera() { SetScript(this); }
	void Update(Win32Window* window, float delta) override;

private:
	float sumX = 0, sumY = 0;
};